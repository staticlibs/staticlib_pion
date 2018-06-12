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

#ifndef STATICLIB_PION_HTTP_REQUEST_HPP
#define STATICLIB_PION_HTTP_REQUEST_HPP

#include <functional>
#include <string>

#include "staticlib/config.hpp"

#include "staticlib/pion/http_message.hpp"
#include "staticlib/pion/http_parser.hpp"

namespace staticlib { 
namespace pion {

/**
 * Container for HTTP request information
 */
class http_request : public http_message {

    /**
     * Request method (GET, POST, PUT, etc.)
     */
    std::string m_method;

    /**
     * Name of the resource or uri-stem to be delivered
     */
    std::string m_resource;

    /**
     * Name of the resource or uri-stem originally requested
     */
    std::string m_original_resource;

    /**
     * Query string portion of the URI
     */
    std::string m_query_string;

    /**
     * HTTP query parameters parsed from the request line and post content
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to> m_query_params;

    /**
     * Payload handler used with this request
     */
    http_parser::payload_handler_type m_payload_handler;

    /**
     * Non-owning pointer to request_reader to be used during parsing
     */
    http_parser* m_request_reader;    

public:

    /**
     * Constructs a new request object (default constructor)
     */
    http_request() :
    m_method(REQUEST_METHOD_GET) { }

    /**
     * Virtual destructor
     */
    virtual ~http_request() STATICLIB_NOEXCEPT { }

    /**
     * Clears all request data
     */
    virtual void clear() {
        http_message::clear();
        m_method.erase();
        m_resource.erase();
        m_original_resource.erase();
        m_query_string.erase();
        m_query_params.clear();
    }

    /**
     * The content length of the message can never be implied for requests
     * 
     * @return false
     */
    virtual bool is_content_length_implied() const {
        return false;
    }

    /**
     * Returns the request method (i.e. GET, POST, PUT)
     * 
     * @return request method
     */
    const std::string& get_method() const {
        return m_method;
    }

    /**
     * Returns the resource uri-stem to be delivered (possibly the result of a redirect)
     * 
     * @return resource uri-stem to be delivered
     */
    const std::string& get_resource() const {
        return m_resource;
    }

    /**
     * Returns the resource uri-stem originally requested
     * 
     * @return resource uri-stem originally requested
     */
    const std::string& get_original_resource() const {
        return m_original_resource;
    }

    /**
     * Returns the uri-query or query string requested
     * 
     * @return uri-query or query string requested
     */
    const std::string& get_query_string() const {
        return m_query_string;
    }

    /**
     * Returns a value for the query key if any are defined; otherwise, an empty string
     * 
     * @param key query key
     * @return value for the query key if any are defined; otherwise, an empty string
     */
    const std::string& get_query(const std::string& key) const {
        return get_value(m_query_params, key);
    }

    /**
     * Returns the query parameters
     * 
     * @return query parameters multimap
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& get_queries() {
        return m_query_params;
    }

    /**
     * Returns true if at least one value for the query key is defined
     * 
     * @param key query key
     * @return true if at least one value for the query key is defined
     */
    bool has_query(const std::string& key) const {
        return (m_query_params.find(key) != m_query_params.end());
    }

    /**
     * Sets the HTTP request method (i.e. GET, POST, PUT)
     * 
     * @param str request method
     */
    void set_method(const std::string& str) {
        m_method = str;
        clear_first_line();
    }

    /**
     * Sets the resource or uri-stem originally requested
     */
    void set_resource(const std::string& str) {
        m_resource = m_original_resource = str;
        clear_first_line();
    }

    /**
     * Changes the resource or uri-stem to be delivered (called as the result of a redirect)
     * 
     * @param str resource or uri-stem to be delivered
     */
    void change_resource(const std::string& str) {
        m_resource = str;
    }

    /**
     * Sets the uri-query or query string requested
     * 
     * @param str uri-query or query string requested
     */
    void set_query_string(const std::string& str) {
        m_query_string = str;
        clear_first_line();
    }

    /**
     * Adds a value for the query key
     * 
     * @param key query key
     * @param value additional value for this key
     */
    void add_query(const std::string& key, const std::string& value) {
        m_query_params.insert(std::make_pair(key, value));
    }

    /**
     * Changes the value of a query key
     * 
     * @param key query key
     * @param value value for this key
     */
    void change_query(const std::string& key, const std::string& value) {
        change_value(m_query_params, key, value);
    }

    /**
     * Removes all values for a query key
     * 
     * @param key query key
     */
    void delete_query(const std::string& key) {
        delete_value(m_query_params, key);
    }

    /**
     * Use the query parameters to build a query string for the request
     */
    void use_query_params_for_query_string() {
        set_query_string(make_query_string(m_query_params));
    }

    /**
     * Use the query parameters to build POST content for the request
     */
    void use_query_params_for_post_content() {
        std::string post_content(make_query_string(m_query_params));
        set_content_length(post_content.size());
        char *ptr = create_content_buffer(); // null-terminates buffer
        if (!post_content.empty()) {
            memcpy(ptr, post_content.c_str(), post_content.size());
        }
        set_method(REQUEST_METHOD_POST);
        set_content_type(CONTENT_TYPE_URLENCODED);
    }

    /**
     * Add content (for POST) from string
     * 
     * @param value POST content
     */
    void set_content(const std::string &value) {
        set_content_length(value.size());
        char *ptr = create_content_buffer();
        if (!value.empty()) {
            memcpy(ptr, value.c_str(), value.size());
        }
    }

    /**
     * Add content (for POST) from buffer of given size;
     * Does nothing if the buffer is invalid or the buffer size is zero
     */
    void set_content(const char* value, size_t size){
        if (nullptr == value || 0 == size) return;
        set_content_length(size);
        char *ptr = create_content_buffer();
        std::memcpy(ptr, value, size);
    }

    /**
     * This method should be called from payload_handler_creator
     * to stick payload handler to this request
     * 
     * @param ph payload handler
     */
    void set_payload_handler(http_parser::payload_handler_type ph) {
        m_payload_handler = std::move(ph);
        // this should be done only once during request processing
        // and we won't restrict request_reader lifecycle after 
        // payload is processed, so we drop the pointer to it
        if (m_request_reader) {
            // request will always outlive reader
            m_request_reader->set_payload_handler(m_payload_handler);
            m_request_reader = nullptr;
        }
    }

    /**
     * Access to the wrapped payload handler object
     * 
     * @return unwrapped payload handler object
     */
    template<typename T>
    T* get_payload_handler() {
        return m_payload_handler.target<T>();
    }

    /**
     * Access to the payload handler wrapper
     * 
     * @return payload handler
     */
    http_parser::payload_handler_type& get_payload_handler_wrapper() {
        return m_payload_handler;
    }
    
    /**
     * Internal method used with payload handlers
     */
    void set_request_reader(http_parser* rr) {
        m_request_reader = rr;
    }

protected:

    /**
     * Updates the string containing the first line for the HTTP message
     */
    virtual void update_first_line() const {
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

    /**
     * Appends HTTP headers for any cookies defined by the http::message
     */
    virtual void append_cookie_headers() {
        for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>::const_iterator i = get_cookies().begin(); i != get_cookies().end(); ++i) {
            std::string cookie_header;
            cookie_header = i->first;
            cookie_header += COOKIE_NAME_VALUE_DELIMITER;
            cookie_header += i->second;
            add_header(HEADER_COOKIE, cookie_header);
        }
    }

};

/**
 * Data type for a HTTP request pointer
 */
using http_request_ptr = std::unique_ptr<http_request>;


} // namespace
}

#endif // STATICLIB_PION_HTTP_REQUEST_HPP
