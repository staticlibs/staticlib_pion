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

#include <memory>

#include "pion/config.hpp"
#include "pion/algorithm.hpp"
#include "pion/http/message.hpp"
#include "pion/http/request.hpp"
#include "pion/http/response.hpp"

namespace pion {
namespace http {

response::response(const http::request& http_request_ptr) : 
m_status_code(RESPONSE_CODE_OK),
m_status_message(RESPONSE_MESSAGE_OK) {
    update_request_info(http_request_ptr);
}

response::response(const std::string& request_method) : 
m_status_code(RESPONSE_CODE_OK), 
m_status_message(RESPONSE_MESSAGE_OK),
m_request_method(request_method) { }

response::response(const response& http_response): 
message(http_response),
m_status_code(http_response.m_status_code),
m_status_message(http_response.m_status_message),
m_request_method(http_response.m_request_method) { }

response::response() : 
m_status_code(RESPONSE_CODE_OK),
m_status_message(RESPONSE_MESSAGE_OK) { }

response::~response() { }

void response::clear() {
    http::message::clear();
    m_status_code = RESPONSE_CODE_OK;
    m_status_message = RESPONSE_MESSAGE_OK;
    m_request_method.clear();
}

bool response::is_content_length_implied() const {
    return (m_request_method == REQUEST_METHOD_HEAD // HEAD responses have no content
            || (m_status_code >= 100 && m_status_code <= 199) // 1xx responses have no content
            || m_status_code == 204 || m_status_code == 205 // no content & reset content responses
            || m_status_code == 304 // not modified responses have no content
            );
}

void response::update_request_info(const http::request& http_request) {
    m_request_method = http_request.get_method();
    if (http_request.get_version_major() == 1 && http_request.get_version_minor() >= 1) {
        set_chunks_supported(true);
    } else if (http_request.get_version_major() == 0) {
        // the request is likely HTTP 0.9 "simple-request", so expect the response to contain no header and no version info
        set_status_code(0U);
        set_status_message("");
        set_version_major(0);
        set_version_minor(0);
    }
}

void response::set_status_code(unsigned int n) {
    m_status_code = n;
    clear_first_line();
}

void response::set_status_message(const std::string& msg) {
    m_status_message = msg;
    clear_first_line();
}

unsigned int response::get_status_code() const {
    return m_status_code;
}

const std::string& response::get_status_message() const {
    return m_status_message;
}

void response::set_cookie(const std::string& name, const std::string& value) {
    std::string set_cookie_header(make_set_cookie_header(name, value, "/"));
    add_header(HEADER_SET_COOKIE, set_cookie_header);
}

void response::set_cookie(const std::string& name, const std::string& value,
        const std::string& path) {
    std::string set_cookie_header(make_set_cookie_header(name, value, path));
    add_header(HEADER_SET_COOKIE, set_cookie_header);
}

void response::set_cookie(const std::string& name, const std::string& value,
        const std::string& path, const unsigned long max_age) {
    std::string set_cookie_header(make_set_cookie_header(name, value, path, true, max_age));
    add_header(HEADER_SET_COOKIE, set_cookie_header);
}

void response::set_cookie(const std::string& name, const std::string& value, const unsigned long max_age) {
    std::string set_cookie_header(make_set_cookie_header(name, value, "/", true, max_age));
    add_header(HEADER_SET_COOKIE, set_cookie_header);
}

void response::delete_cookie(const std::string& name) {
    std::string set_cookie_header(make_set_cookie_header(name, "", "/", true, 0));
    add_header(HEADER_SET_COOKIE, set_cookie_header);
}

void response::delete_cookie(const std::string& name, const std::string& path) {
    std::string set_cookie_header(make_set_cookie_header(name, "", path, true, 0));
    add_header(HEADER_SET_COOKIE, set_cookie_header);
}

void response::set_last_modified(const unsigned long t) {
    change_header(HEADER_LAST_MODIFIED, get_date_string(t));
}

void response::update_first_line() const {
    // start out with the HTTP version
    m_first_line = get_version_string();
    m_first_line += ' ';
    // append the response status code
    m_first_line += pion::algorithm::to_string(m_status_code);
    m_first_line += ' ';
    // append the response status message
    m_first_line += m_status_message;
}

void response::append_cookie_headers() {
    for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = get_cookies().begin(); i != get_cookies().end(); ++i) {
        set_cookie(i->first, i->second);
    }
}   

} // end namespace http
} // end namespace pion
