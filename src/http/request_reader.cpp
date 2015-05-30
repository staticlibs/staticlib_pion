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

#include "pion/config.hpp"
#include "pion/http/request.hpp"
#include "pion/http/reader.hpp"
#include "pion/http/request_reader.hpp"

namespace pion {
namespace http {

request_reader::~request_reader() { }

std::shared_ptr<request_reader> request_reader::create(tcp::connection_ptr& tcp_conn, 
        finished_handler_t handler) {
    return std::shared_ptr<request_reader>(new request_reader(tcp_conn, handler));
}

void request_reader::set_headers_parsed_callback(headers_parsing_finished_handler_t& h) {
    m_parsed_headers = h;
}

request_reader::request_reader(tcp::connection_ptr& tcp_conn, finished_handler_t handler) : 
http::reader(true, tcp_conn), 
m_http_msg(new http::request),
m_finished(handler) {
    m_http_msg->set_remote_ip(tcp_conn->get_remote_ip());
    m_http_msg->set_request_reader(this);
    set_logger(PION_GET_LOGGER("pion.http.request_reader"));
}

void request_reader::read_bytes(void) {
    auto reader = shared_from_this();
    get_connection()->async_read_some([reader](const asio::error_code& read_error, 
            std::size_t bytes_read) {
        reader->consume_bytes(read_error, bytes_read);
    });
}

void request_reader::finished_parsing_headers(const asio::error_code& ec, pion::tribool& rc) {
    // call the finished headers handler with the HTTP message
    if (m_parsed_headers) m_parsed_headers(m_http_msg, get_connection(), ec, rc);
}

void request_reader::finished_reading(const asio::error_code& ec) {
    // call the finished handler with the finished HTTP message
    if (m_finished) m_finished(m_http_msg, get_connection(), ec);
}

http::message& request_reader::get_message() {
    return *m_http_msg;
}

} // end namespace http
} // end namespace pion
