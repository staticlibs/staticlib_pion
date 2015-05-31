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
#include "pion/http/writer.hpp"

namespace pion {
namespace http {

writer::writer(tcp::connection_ptr& tcp_conn, finished_handler_t handler) :
m_logger(PION_GET_LOGGER("pion.http.writer")),
m_tcp_conn(tcp_conn),
m_content_length(0),
m_stream_is_empty(true),
m_client_supports_chunks(true),
m_sending_chunks(false),
m_sent_headers(false),
m_finished(handler) { }

writer::~writer() { }

// writer member functions

void writer::prepare_write_buffers(http::message::write_buffers_t& write_buffers,
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
            write_buffers.push_back(asio::buffer(http::types::STRING_CRLF));
            
            // append response content buffers
            write_buffers.insert(write_buffers.end(), m_content_buffers.begin(),
                                 m_content_buffers.end());
            // append an extra CRLF for chunk formatting
            write_buffers.push_back(asio::buffer(http::types::STRING_CRLF));
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
        write_buffers.push_back(asio::buffer(http::types::STRING_CRLF));
        write_buffers.push_back(asio::buffer(http::types::STRING_CRLF));
    }
}

void writer::finished_writing(const asio::error_code& ec) {
    if (m_finished) m_finished(ec);
}

void writer::clear() {
    m_content_buffers.clear();
    m_binary_cache.clear();
    m_text_cache.clear();
    m_content_stream.str("");
    m_stream_is_empty = true;
    m_content_length = 0;
}

void writer::write(std::ostream& (*iomanip)(std::ostream&)) {
    m_content_stream << iomanip;
    if (m_stream_is_empty) m_stream_is_empty = false;
}

void writer::write(const void *data, size_t length) {
    if (length != 0) {
        flush_content_stream();
        m_content_buffers.push_back(m_binary_cache.add(data, length));
        m_content_length += length;
    }
}

void writer::write_no_copy(const std::string& data) {
    if (!data.empty()) {
        flush_content_stream();
        m_content_buffers.push_back(asio::buffer(data));
        m_content_length += data.size();
    }
}

void writer::write_no_copy(void *data, size_t length) {
    if (length > 0) {
        flush_content_stream();
        m_content_buffers.push_back(asio::buffer(data, length));
        m_content_length += length;
    }
}

void writer::write_move(std::string&& data) {
    if (!data.empty()) {
        m_moved_cache.emplace_back(std::move(data));
        std::string& dataref = m_moved_cache.back();
        flush_content_stream();
        m_content_buffers.emplace_back(dataref.c_str(), dataref.length());
        m_content_length += dataref.length();
    }
}

void writer::send() {
    send_more_data(false, bind_to_write_handler());
}

void writer::send_final_chunk() {
    m_sending_chunks = true;
    send_more_data(true, bind_to_write_handler());
}

tcp::connection_ptr& writer::get_connection() {
    return m_tcp_conn;
}

size_t writer::get_content_length() const {
    return m_content_length;
}

void writer::supports_chunked_messages(bool b) {
    m_client_supports_chunks = b;
}

bool writer::supports_chunked_messages() const {
    return m_client_supports_chunks;
}

bool writer::sending_chunked_message() const {
    return m_sending_chunks;
}

void writer::set_logger(logger log_ptr) {
    m_logger = log_ptr;
}

logger writer::get_logger() {
    return m_logger;
}

void writer::flush_content_stream() {
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

writer::binary_cache_t::~binary_cache_t() {
    for (iterator i = begin(); i != end(); ++i) {
        delete[] i->first;
    }
}

asio::const_buffer writer::binary_cache_t::add(const void *ptr, const size_t size) {
    char *data_ptr = new char[size];
    memcpy(data_ptr, ptr, size);
    push_back(std::make_pair(data_ptr, size));
    return asio::buffer(data_ptr, size);
}

    
} // end namespace http
} // end namespace pion
