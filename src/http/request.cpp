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

#include "pion/http/request.hpp"


namespace pion {
namespace http {

request::request(const std::string& resource) : 
m_method(REQUEST_METHOD_GET), 
m_resource(resource) { }

request::request() : 
m_method(REQUEST_METHOD_GET) { }

request::~request() { }

void request::clear() {
    http::message::clear();
    m_method.erase();
    m_resource.erase();
    m_original_resource.erase();
    m_query_string.erase();
    m_query_params.clear();
}

bool request::is_content_length_implied() const {
    return false;
}

const std::string& request::get_method(void) const {
    return m_method;
}

const std::string& request::get_resource() const {
    return m_resource;
}

const std::string& request::get_original_resource() const {
    return m_original_resource;
}

const std::string& request::get_query_string() const {
    return m_query_string;
}

const std::string& request::get_query(const std::string& key) const {
    return get_value(m_query_params, key);
}

std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& request::get_queries() {
    return m_query_params;
}

bool request::has_query(const std::string& key) const {
    return (m_query_params.find(key) != m_query_params.end());
}

void request::set_method(const std::string& str) {
    m_method = str;
    clear_first_line();
}

void request::set_resource(const std::string& str) {
    m_resource = m_original_resource = str;
    clear_first_line();
}

void request::change_resource(const std::string& str) {
    m_resource = str;
}

void request::set_query_string(const std::string& str) {
    m_query_string = str;
    clear_first_line();
}

void request::add_query(const std::string& key, const std::string& value) {
    m_query_params.insert(std::make_pair(key, value));
}

void request::change_query(const std::string& key, const std::string& value) {
    change_value(m_query_params, key, value);
}

void request::delete_query(const std::string& key) {
    delete_value(m_query_params, key);
}

void request::use_query_params_for_query_string() {
    set_query_string(make_query_string(m_query_params));
}

void request::use_query_params_for_post_content() {
    std::string post_content(make_query_string(m_query_params));
    set_content_length(post_content.size());
    char *ptr = create_content_buffer(); // null-terminates buffer
    if (!post_content.empty()) {
        memcpy(ptr, post_content.c_str(), post_content.size());
    }
    set_method(REQUEST_METHOD_POST);
    set_content_type(CONTENT_TYPE_URLENCODED);
}

void request::set_content(const std::string &value) {
    set_content_length(value.size());
    char *ptr = create_content_buffer();
    if (!value.empty()) {
        memcpy(ptr, value.c_str(), value.size());
    }
}

void request::set_content(const char* value, size_t size) {
    if (NULL == value || 0 == size) return;
    set_content_length(size);
    char *ptr = create_content_buffer();
    memcpy(ptr, value, size);
}

void request::set_payload_handler(parser::payload_handler_t ph) {
    m_payload_handler = std::move(ph);
    // this should be done only once during request processing
    // and we won't restrict request_reader lifecycle after 
    // payload is processed, so we drop the pointer to it
    if (m_request_reader) {
        // request will always outlive reader
        m_request_reader->set_payload_handler(m_payload_handler);
        m_request_reader = NULL;
    }
}

parser::payload_handler_t& request::get_payload_handler_wrapper() {
    return m_payload_handler;
}

void request::set_request_reader(parser* rr) {
    m_request_reader = rr;
}

void request::update_first_line() const {
    // start out with the request method
    m_first_line = m_method;
    m_first_line += ' ';
    // append the resource requested
    m_first_line += m_resource;
    if (!m_query_string.empty()) {
        // append query string if not empty
        m_first_line += '?';
        m_first_line += m_query_string;
    }
    m_first_line += ' ';
    // append HTTP version
    m_first_line += get_version_string();
}

void request::append_cookie_headers() {
    for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>::const_iterator i = get_cookies().begin(); i != get_cookies().end(); ++i) {
        std::string cookie_header;
        cookie_header = i->first;
        cookie_header += COOKIE_NAME_VALUE_DELIMITER;
        cookie_header += i->second;
        add_header(HEADER_COOKIE, cookie_header);
    }
}

} // end namespace http
} // end namespace pion
