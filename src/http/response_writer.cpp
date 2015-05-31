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

#include <functional>
#include <memory>

#include "asio.hpp"

#include "pion/noncopyable.hpp"
#include "pion/config.hpp"
#include "pion/http/writer.hpp"
#include "pion/http/request.hpp"
#include "pion/http/response.hpp"
#include "pion/http/response_writer.hpp"

namespace pion {
namespace http {

response_writer::~response_writer() { }

std::shared_ptr<response_writer> response_writer::create(tcp::connection_ptr& tcp_conn,
        const http::request& http_request, finished_handler_t handler) {
    return std::shared_ptr<response_writer>(new response_writer(tcp_conn, http_request, handler));
}

http::response& response_writer::get_response() {
    return *m_http_response;
}

response_writer::response_writer(tcp::connection_ptr& tcp_conn, http::response_ptr& http_response_ptr,
        finished_handler_t handler) : 
http::writer(tcp_conn, handler),
m_http_response(http_response_ptr) {
    set_logger(PION_GET_LOGGER("pion.http.response_writer"));
    // tell the http::writer base class whether or not the client supports chunks
    supports_chunked_messages(m_http_response->get_chunks_supported());
    // check if we should initialize the payload content using
    // the response's content buffer
    if (m_http_response->get_content_length() > 0
            && m_http_response->get_content() != NULL
            && m_http_response->get_content()[0] != '\0') {
        write_no_copy(m_http_response->get_content(), m_http_response->get_content_length());
    }
}

response_writer::response_writer(tcp::connection_ptr& tcp_conn, const http::request& http_request,
        finished_handler_t handler) : 
http::writer(tcp_conn, handler),
m_http_response(new http::response(http_request)) {
    set_logger(PION_GET_LOGGER("pion.http.response_writer"));
    // tell the http::writer base class whether or not the client supports chunks
    supports_chunked_messages(m_http_response->get_chunks_supported());
}

void response_writer::prepare_buffers_for_send(http::message::write_buffers_t& write_buffers) {
    if (get_content_length() > 0) {
        m_http_response->set_content_length(get_content_length());
    }
    m_http_response->prepare_buffers_for_send(write_buffers, get_connection()->get_keep_alive(),
            sending_chunked_message());
}

writer::write_handler_t response_writer::bind_to_write_handler() {
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
