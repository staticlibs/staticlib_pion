/*
 * Copyright 2015, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "staticlib/pion/http_request_reader.hpp"

#include "asio.hpp"

namespace staticlib { 
namespace pion {

namespace { // anonymous

const std::string log = "staticlib.pion.http_request_reader";

} // namespace

const uint32_t http_request_reader::DEFAULT_READ_TIMEOUT_MILLIS = 10000;

tcp_connection_ptr& http_request_reader::get_connection() {
    return m_tcp_conn;
}

void http_request_reader::set_timeout(std::chrono::milliseconds timeout) {
    m_read_timeout_millis = static_cast<uint32_t> (timeout.count());
}

// reader member functions

void http_request_reader::receive() {
    if (m_tcp_conn->get_pipelined()) {
        // there are pipelined messages available in the connection's read buffer
        m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // default to close the connection
        m_tcp_conn->load_read_pos(m_read_ptr, m_read_end_ptr);
        consume_bytes();
    } else {
        // no pipelined messages available in the read buffer -> read bytes from the socket
        m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // default to close the connection
        read_bytes_with_timeout();
    }
}

void http_request_reader::consume_bytes(const std::error_code& read_error, std::size_t bytes_read) {
    // cancel read timer if operation didn't time-out
    if (m_timer_ptr) {
        m_timer_ptr->cancel();
        m_timer_ptr.reset();
    }

    if (read_error) {
        // a read error occured
        handle_read_error(read_error);
        return;
    }

    STATICLIB_PION_LOG_DEBUG(log, "Read " << bytes_read << " bytes from HTTP request");

    // set pointers for new HTTP header data to be consumed
    set_read_buffer(m_tcp_conn->get_read_buffer().data(), bytes_read);

    consume_bytes();
}

void http_request_reader::consume_bytes() {
    // parse the bytes read from the last operation
    //
    // note that tribool may have one of THREE states:
    //
    // false: encountered an error while parsing message
    // true: finished successfully parsing the message
    // indeterminate: parsed bytes, but the message is not yet finished
    //
    std::error_code ec;
    sl::support::tribool result = parse(get_message(), ec);

    if (gcount() > 0) {
        // parsed > 0 bytes in HTTP headers
        STATICLIB_PION_LOG_DEBUG(log, "Parsed " << gcount() << " HTTP bytes");
    }

    if (result == true) {
        // finished reading HTTP message and it is valid

        // set the connection's lifecycle type
        if (get_message().check_keep_alive()) {
            if (eof()) {
                // the connection should be kept alive, but does not have pipelined messages
                m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_KEEPALIVE);
            } else {
                // the connection has pipelined messages
                m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_PIPELINED);

                // save the read position as a bookmark so that it can be retrieved
                // by a new HTTP parser, which will be created after the current
                // message has been handled
                m_tcp_conn->save_read_pos(m_read_ptr, m_read_end_ptr);

                STATICLIB_PION_LOG_DEBUG(log, "HTTP pipelined request("
                        << bytes_available() << " bytes available)");
            }
        } else {
            m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE);
        }

        // we have finished parsing the HTTP message
        finished_reading(ec);

    } else if (result == false) {
        // the message is invalid or an error occured
        m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // make sure it will get closed
        get_message().set_is_valid(false);
        finished_reading(ec);
    } else {
        // not yet finished parsing the message -> read more data
        read_bytes_with_timeout();
    }
}

void http_request_reader::read_bytes_with_timeout() {
    if (m_read_timeout_millis > 0) {
        m_timer_ptr.reset(new sl::concurrent::cancelable_timer<asio::steady_timer>(
                sl::support::make_unique<asio::steady_timer>(m_tcp_conn->get_io_service())));
        auto conn = m_tcp_conn;
        m_timer_ptr->start(std::chrono::milliseconds(m_read_timeout_millis), [conn] (const std::error_code&) {
            conn->cancel();
        });
    } else if (m_timer_ptr) {
        m_timer_ptr.reset();
    }
    read_bytes();
}

void http_request_reader::handle_read_error(const std::error_code& read_error) {
    // close the connection, forcing the client to establish a new one
    m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // make sure it will get closed

    // check if this is just a message with unknown content length
    if (!check_premature_eof(get_message())) {
        std::error_code ec; // clear error code
        finished_reading(ec);
        return;
    }

    // only log errors if the parsing has already begun
    if (get_total_bytes_read() > 0) {
        if (read_error == asio::error::operation_aborted) {
            // if the operation was aborted, the acceptor was stopped,
            // which means another thread is shutting-down the server
            STATICLIB_PION_LOG_INFO(log, "HTTP request parsing aborted (shutting down)");
        } else {
            STATICLIB_PION_LOG_INFO(log, "HTTP request parsing aborted (" << read_error.message() << ')');
        }
    }

    finished_reading(read_error);
}

http_request_reader::http_request_reader(tcp_connection_ptr& tcp_conn,
            headers_parsing_finished_handler_type headers_parsed_cb,
            finished_handler_type received_cb) :
http_parser(true),
m_tcp_conn(tcp_conn),
m_read_timeout_millis(DEFAULT_READ_TIMEOUT_MILLIS),
m_http_msg(new http_request),
m_parsed_headers(std::move(headers_parsed_cb)),
m_finished(std::move(received_cb)) {
    m_http_msg->set_remote_ip(tcp_conn->get_remote_ip());
    m_http_msg->set_request_reader(this);
}

void http_request_reader::read_bytes(void) {
    auto reader = shared_from_this();
    get_connection()->async_read_some([reader](const std::error_code& read_error, 
            std::size_t bytes_read) {
        reader->consume_bytes(read_error, bytes_read);
        });
}

void http_request_reader::finished_parsing_headers(const std::error_code& ec, sl::support::tribool& rc) {
    // call the finished headers handler with the HTTP message
    if (m_parsed_headers) {
        m_parsed_headers(m_http_msg, get_connection(), ec, rc);
    }
}

void http_request_reader::finished_reading(const std::error_code& ec) {
    // call the finished handler with the finished HTTP message
    if (m_finished) {
        m_finished(std::move(m_http_msg), get_connection(), ec);
    }
}

http_message& http_request_reader::get_message() {
    return *m_http_msg;
}
} // namespace
}
