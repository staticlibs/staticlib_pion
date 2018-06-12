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

#include "staticlib/pion/http_response_writer.hpp"

#include <array>

#include "staticlib/support.hpp"

namespace staticlib {
namespace pion {

namespace { // anonymous

const std::string log = "staticlib.pion.http_response_writer";

} // namespace

http_response_writer::http_response_writer(tcp_connection_ptr& tcp_conn, const http_request& http_request) :
m_tcp_conn(tcp_conn),
m_content_length(0),
m_client_supports_chunks(true),
m_sending_chunks(false),
m_sent_headers(false),
m_finished([tcp_conn](const std::error_code&) { tcp_conn->finish(); }),
m_http_response(new http_response(http_request)) {
    // set whether or not the client supports chunks
    supports_chunked_messages(m_http_response->get_chunks_supported());
}

// writer member functions

void http_response_writer::prepare_write_buffers(http_message::write_buffers_type& write_buffers,
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
            auto cast_buf = std::array<char, 35>();
            auto written = sprintf(cast_buf.data(), "%lx", static_cast<long>(m_content_length));

            // add chunk length as a string at the back of the text cache
            // append length of chunk to write_buffers
            write_buffers.push_back(add_to_cache({cast_buf.data(), written}));
            // append an extra CRLF for chunk formatting
            write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));

            // append response content buffers
            write_buffers.insert(write_buffers.end(), m_content_buffers.begin(),
                                 m_content_buffers.end());
            // append an extra CRLF for chunk formatting
            write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));
        } else {
            // append response content buffers
            write_buffers.insert(write_buffers.end(), m_content_buffers.begin(),
                                 m_content_buffers.end());
        }
    }

    // prepare a zero-byte (final) chunk
    if (send_final_chunk && supports_chunked_messages() && sending_chunked_message()) {
        // add chunk length as a string at the back of the text cache
        char zero = '0';
        // append length of chunk to write_buffers
        write_buffers.push_back(add_to_cache({std::addressof(zero), 1}));
        // append an extra CRLF for chunk formatting
        write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));
        write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));
    }
}

void http_response_writer::finished_writing(const std::error_code& ec) {
    if (m_http_response->is_body_allowed()) {
        if (m_finished) {
            m_finished(ec);
        }
    }
}

void http_response_writer::clear() {
    m_content_buffers.clear();
    payload_cache.clear();
    m_content_length = 0;
}

void http_response_writer::write(sl::io::span<const char> data) {
    if (m_http_response->is_body_allowed() && data.size() > 0) {
        m_content_buffers.push_back(add_to_cache(data));
        m_content_length += data.size();
    }
}

void http_response_writer::write_nocopy(sl::io::span<const char> data) {
    if (m_http_response->is_body_allowed() && data.size() > 0) {
        m_content_buffers.push_back(asio::buffer(data.data(), data.size()));
        m_content_length += data.size();
    }
}

void http_response_writer::send(std::unique_ptr<http_response_writer> self) {
    auto self_ptr = self.get();
    self_ptr->send_more_data(false, bind_to_write_handler(std::move(self)));
}

void http_response_writer::send_final_chunk(std::unique_ptr<http_response_writer> self) {
    self->m_sending_chunks = true;
    auto self_ptr = self.get();
    self_ptr->send_more_data(true, bind_to_write_handler(std::move(self)));
}

tcp_connection_ptr& http_response_writer::get_connection() {
    return m_tcp_conn;
}

size_t http_response_writer::get_content_length() const {
    return m_content_length;
}

void http_response_writer::supports_chunked_messages(bool b) {
    m_client_supports_chunks = b;
}

bool http_response_writer::supports_chunked_messages() const {
    return m_client_supports_chunks;
}

bool http_response_writer::sending_chunked_message() const {
    return m_sending_chunks;
}

asio::const_buffer http_response_writer::add_to_cache(sl::io::span<const char> data) {
    payload_cache.emplace_back(new char[data.size()]);
    char* dest = payload_cache.back().get();
    std::memcpy(dest, data.data(), data.size());
    return asio::buffer(dest, data.size());
}

http_response& http_response_writer::get_response() {
    return *m_http_response;
}

void http_response_writer::prepare_buffers_for_send(http_message::write_buffers_type& write_buffers) {
    if (get_content_length() > 0) {
        m_http_response->set_content_length(get_content_length());
    }
    m_http_response->prepare_buffers_for_send(write_buffers, get_connection()->get_keep_alive(),
            sending_chunked_message());
}

http_response_writer::write_handler_type http_response_writer::bind_to_write_handler(
        std::unique_ptr<http_response_writer> self) {
    auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
    return [self_shared](const std::error_code& ec, std::size_t bt) { 
        auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
        if (nullptr != self.get()) {
            handle_write(std::move(self), ec, bt); 
        } else {
            STATICLIB_PION_LOG_WARN(log, "Lost context detected in 'async_write'");
        }
    };
}

void http_response_writer::handle_write(std::unique_ptr<http_response_writer> self,
        const std::error_code& ec, std::size_t bytes_written) {
    if (!ec) {
        // response sent OK
        if (self->sending_chunked_message()) {
            STATICLIB_PION_LOG_DEBUG(log, "Sent HTTP response chunk of " << bytes_written << " bytes");
        } else {
            STATICLIB_PION_LOG_DEBUG(log,
                    "Sent HTTP response of " << bytes_written << " bytes ("
                    << (self->get_connection()->get_keep_alive() ? "keeping alive)" : "closing)"));
        }
    }
    self->finished_writing(ec);
}

} // namespace
}
