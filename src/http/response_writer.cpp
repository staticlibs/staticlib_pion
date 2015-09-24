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

// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include "asio.hpp"

#include "pion/http/message.hpp"
#include "pion/http/response_writer.hpp"

namespace pion {
namespace http {


response_writer::response_writer(tcp::connection_ptr& tcp_conn, const http::request& http_request,
        finished_handler_t handler) :
m_logger(PION_GET_LOGGER("pion.http.writer")),
m_tcp_conn(tcp_conn),
m_content_length(0),
m_stream_is_empty(true),
m_client_supports_chunks(true),
m_sending_chunks(false),
m_sent_headers(false),
m_finished(handler),
m_http_response(new http::response(http_request)) {
    set_logger(PION_GET_LOGGER("pion.http.response_writer"));
    // set whether or not the client supports chunks
    supports_chunked_messages(m_http_response->get_chunks_supported());
}

// writer member functions

std::shared_ptr<response_writer> response_writer::create(http::request_ptr& http_request, tcp::connection_ptr& tcp_conn) {
    return create(tcp_conn, *http_request, std::bind(&tcp::connection::finish, tcp_conn));
}

std::shared_ptr<response_writer> response_writer::create(tcp::connection_ptr& tcp_conn,
        const http::request& http_request, finished_handler_t handler) {
    return std::shared_ptr<response_writer>(new response_writer(tcp_conn, http_request, handler));
}

void response_writer::prepare_write_buffers(http::message::write_buffers_t& write_buffers,
        const bool send_final_chunk) {
    // check if the HTTP headers have been sent yet
    if (! m_sent_headers) {
        // initialize write buffers for send operation
        prepare_buffers_for_send(write_buffers);

        // only send the headers once
        m_sent_headers = true;
    }

    // combine I/O write buffers (headers and content) so that everything
    // can be sent together; otherwise, we would have to send headers
    // and content separately, which would not be as efficient
    
    // don't send anything if there is no data in content buffers
    if (m_content_length > 0) {
        if (supports_chunked_messages() && sending_chunked_message()) {
            // prepare the next chunk of data to send
            // write chunk length in hex
            char cast_buf[35];
            sprintf(cast_buf, "%lx", static_cast<long>(m_content_length));
            
            // add chunk length as a string at the back of the text cache
            m_text_cache.push_back(cast_buf);
            // append length of chunk to write_buffers
            write_buffers.push_back(asio::buffer(m_text_cache.back()));
            // append an extra CRLF for chunk formatting
            write_buffers.push_back(asio::buffer(http::message::STRING_CRLF));
            
            // append response content buffers
            write_buffers.insert(write_buffers.end(), m_content_buffers.begin(),
                                 m_content_buffers.end());
            // append an extra CRLF for chunk formatting
            write_buffers.push_back(asio::buffer(http::message::STRING_CRLF));
        } else {
            // append response content buffers
            write_buffers.insert(write_buffers.end(), m_content_buffers.begin(),
                                 m_content_buffers.end());
        }
    }
    
    // prepare a zero-byte (final) chunk
    if (send_final_chunk && supports_chunked_messages() && sending_chunked_message()) {
        // add chunk length as a string at the back of the text cache
        m_text_cache.push_back("0");
        // append length of chunk to write_buffers
        write_buffers.push_back(asio::buffer(m_text_cache.back()));
        // append an extra CRLF for chunk formatting
        write_buffers.push_back(asio::buffer(http::message::STRING_CRLF));
        write_buffers.push_back(asio::buffer(http::message::STRING_CRLF));
    }
}

void response_writer::finished_writing(const asio::error_code& ec) {
    if (m_finished) m_finished(ec);
}

void response_writer::clear() {
    m_content_buffers.clear();
    m_binary_cache.clear();
    m_text_cache.clear();
    m_content_stream.str("");
    m_stream_is_empty = true;
    m_content_length = 0;
}

void response_writer::write(std::ostream& (*iomanip)(std::ostream&)) {
    m_content_stream << iomanip;
    if (m_stream_is_empty) m_stream_is_empty = false;
}

void response_writer::write(const void *data, size_t length) {
    if (length != 0) {
        flush_content_stream();
        m_content_buffers.push_back(m_binary_cache.add(data, length));
        m_content_length += length;
    }
}

void response_writer::write_no_copy(const std::string& data) {
    if (!data.empty()) {
        flush_content_stream();
        m_content_buffers.push_back(asio::buffer(data));
        m_content_length += data.size();
    }
}

void response_writer::write_no_copy(void *data, size_t length) {
    if (length > 0) {
        flush_content_stream();
        m_content_buffers.push_back(asio::buffer(data, length));
        m_content_length += length;
    }
}

void response_writer::write_move(std::string&& data) {
    if (!data.empty()) {
        m_moved_cache.emplace_back(std::move(data));
        std::string& dataref = m_moved_cache.back();
        flush_content_stream();
        m_content_buffers.emplace_back(dataref.c_str(), dataref.length());
        m_content_length += dataref.length();
    }
}

void response_writer::send() {
    send_more_data(false, bind_to_write_handler());
}

void response_writer::send_final_chunk() {
    m_sending_chunks = true;
    send_more_data(true, bind_to_write_handler());
}

tcp::connection_ptr& response_writer::get_connection() {
    return m_tcp_conn;
}

size_t response_writer::get_content_length() const {
    return m_content_length;
}

void response_writer::supports_chunked_messages(bool b) {
    m_client_supports_chunks = b;
}

bool response_writer::supports_chunked_messages() const {
    return m_client_supports_chunks;
}

bool response_writer::sending_chunked_message() const {
    return m_sending_chunks;
}

void response_writer::set_logger(logger log_ptr) {
    m_logger = log_ptr;
}

logger response_writer::get_logger() {
    return m_logger;
}

void response_writer::flush_content_stream() {
    if (!m_stream_is_empty) {
        std::string string_to_add(m_content_stream.str());
        if (!string_to_add.empty()) {
            m_content_stream.str("");
            m_content_length += string_to_add.size();
            m_text_cache.push_back(string_to_add);
            m_content_buffers.push_back(asio::buffer(m_text_cache.back()));
        }
        m_stream_is_empty = true;
    }
}

response_writer::binary_cache_t::~binary_cache_t() {
    for (iterator i = begin(); i != end(); ++i) {
        delete[] i->first;
    }
}

asio::const_buffer response_writer::binary_cache_t::add(const void *ptr, const size_t size) {
    char *data_ptr = new char[size];
    memcpy(data_ptr, ptr, size);
    push_back(std::make_pair(data_ptr, size));
    return asio::buffer(data_ptr, size);
}

http::response& response_writer::get_response() {
    return *m_http_response;
}

void response_writer::prepare_buffers_for_send(http::message::write_buffers_t& write_buffers) {
    if (get_content_length() > 0) {
        m_http_response->set_content_length(get_content_length());
    }
    m_http_response->prepare_buffers_for_send(write_buffers, get_connection()->get_keep_alive(),
            sending_chunked_message());
}

response_writer::write_handler_t response_writer::bind_to_write_handler() {
    return std::bind(&response_writer::handle_write, shared_from_this(),
            std::placeholders::_1 /* asio::placeholders::error */,
            std::placeholders::_2 /* asio::placeholders::bytes_transferred */);
}

void response_writer::handle_write(const asio::error_code& write_error, std::size_t bytes_written) {
    (void) bytes_written;
    logger log_ptr(get_logger());
    if (!write_error) {
        // response sent OK
        if (sending_chunked_message()) {
            PION_LOG_DEBUG(log_ptr, "Sent HTTP response chunk of " << bytes_written << " bytes");
        } else {
            PION_LOG_DEBUG(log_ptr, "Sent HTTP response of " << bytes_written << " bytes ("
                    << (get_connection()->get_keep_alive() ? "keeping alive)" : "closing)"));
        }
    }
    finished_writing(write_error);
}
    
} // end namespace http
} // end namespace pion
