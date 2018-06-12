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

#include "staticlib/pion/http_server.hpp"

namespace staticlib { 
namespace pion {

namespace { // anonymous

const std::string log = "staticlib.pion.http_request_reader";

} // namespace

// reader member functions

void http_request_reader::receive(std::unique_ptr<http_request_reader> self) {
    if (self->tcp_conn->is_pipelined()) {
        // there are pipelined messages available in the connection's read buffer
        self->tcp_conn->set_lifecycle(tcp_connection::lifecycle::close); // default to close the connection
        self->tcp_conn->load_read_pos(self->m_read_ptr, self->m_read_end_ptr);
        consume_bytes(std::move(self));
    } else {
        // no pipelined messages available in the read buffer -> read bytes from the socket
        self->tcp_conn->set_lifecycle(tcp_connection::lifecycle::close); // default to close the connection
        read_bytes_with_timeout(std::move(self));
    }
}

void http_request_reader::consume_bytes(std::unique_ptr<http_request_reader> self,
        const std::error_code& read_error, std::size_t bytes_read) {
    if (read_error) {
        // a read error occured
        self->handle_read_error(read_error);
        return;
    }

    STATICLIB_PION_LOG_DEBUG(log, "Read " << bytes_read << " bytes from HTTP request");

    // set pointers for new HTTP header data to be consumed
    self->set_read_buffer(self->tcp_conn->get_read_buffer().data(), bytes_read);

    consume_bytes(std::move(self));
}

void http_request_reader::consume_bytes(std::unique_ptr<http_request_reader> self) {
    // parse the bytes read from the last operation
    //
    // note that tribool may have one of THREE states:
    //
    // false: encountered an error while parsing message
    // true: finished successfully parsing the message
    // indeterminate: parsed bytes, but the message is not yet finished
    //
    std::error_code ec;
    sl::support::tribool result = self->parse(*self->request, ec);

    if (self->gcount() > 0) {
        // parsed > 0 bytes in HTTP headers
        STATICLIB_PION_LOG_DEBUG(log, "Parsed " << self->gcount() << " HTTP bytes");
    }

    if (result == true) {
        // finished reading HTTP message and it is valid

        // set the connection's lifecycle type
        if (self->request->check_keep_alive()) {
            if (self->eof()) {
                // the connection should be kept alive, but does not have pipelined messages
                self->tcp_conn->set_lifecycle(tcp_connection::lifecycle::keepalive);
            } else {
                // the connection has pipelined messages
                self->tcp_conn->set_lifecycle(tcp_connection::lifecycle::pipelined);

                // save the read position as a bookmark so that it can be retrieved
                // by a new HTTP parser, which will be created after the current
                // message has been handled
                self->tcp_conn->save_read_pos(self->m_read_ptr, self->m_read_end_ptr);

                STATICLIB_PION_LOG_DEBUG(log, "HTTP pipelined request("
                        << self->bytes_available() << " bytes available)");
            }
        } else {
            self->tcp_conn->set_lifecycle(tcp_connection::lifecycle::close);
        }

        // we have finished parsing the HTTP message
        self->finished_reading(ec);

    } else if (result == false) {
        // the message is invalid or an error occured
        self->tcp_conn->set_lifecycle(tcp_connection::lifecycle::close); // make sure it will get closed
        self->request->set_is_valid(false);
        self->finished_reading(ec);
    } else {
        // not yet finished parsing the message -> read more data
        read_bytes_with_timeout(std::move(self));
    }
}

void http_request_reader::read_bytes_with_timeout(std::unique_ptr<http_request_reader> self) {
    // setup timer
    auto& timer = self->tcp_conn->get_timer();
    auto& strand = self->tcp_conn->get_strand();
    timer.expires_from_now(std::chrono::milliseconds(self->read_timeout_millis));
    auto conn = self->tcp_conn;
    auto timeout_handler =
        [conn] (const std::error_code& ec) {
            if (asio::error::operation_aborted != ec.value()) {
                conn->cancel();
            }
        };
    auto timeout_handler_stranded = strand.wrap(std::move(timeout_handler));
    // setup read
    auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
    auto read_handler =
        [self_shared](const std::error_code& ec, std::size_t bytes_read) {
            auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
            if (nullptr != self.get()) {
                if(asio::error::operation_aborted != ec.value()) {
                    self->tcp_conn->cancel_timer();
                }
                consume_bytes(std::move(self), ec, bytes_read);
            } else {
                STATICLIB_PION_LOG_WARN(log, "Lost context detected in 'async_read_some'");
            }
        };
    auto read_handler_standed = strand.wrap(std::move(read_handler));
    // fire
    conn->async_read_some(std::move(read_handler_standed));
    timer.async_wait(std::move(timeout_handler_stranded));
}

void http_request_reader::handle_read_error(const std::error_code& read_error) {
    // close the connection, forcing the client to establish a new one
    tcp_conn->set_lifecycle(tcp_connection::lifecycle::close); // make sure it will get closed

    // check if this is just a message with unknown content length
    if (!check_premature_eof(*request)) {
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

void http_request_reader::finished_parsing_headers(const std::error_code& ec, sl::support::tribool& rc) {
    server.handle_request_after_headers_parsed(request, tcp_conn, ec, rc);
}

void http_request_reader::finished_reading(const std::error_code& ec) {
    server.handle_request(std::move(request), tcp_conn, ec);
}

} // namespace
}
