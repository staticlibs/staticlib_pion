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

#ifndef STATICLIB_PION_HTTP_RESPONSE_HPP
#define STATICLIB_PION_HTTP_RESPONSE_HPP

#include <memory>

#include "staticlib/config.hpp"

#include "staticlib/pion/algorithm.hpp"
#include "staticlib/pion/http_message.hpp"
#include "staticlib/pion/http_request.hpp"

namespace staticlib { 
namespace pion {

/**
 * Container for HTTP response information
 */
class http_response : public http_message {

    /**
     * The HTTP response status code
     */
    unsigned int status_code;

    /**
     * The HTTP response status message
     */
    std::string status_message;

    /**
     * HTTP method used by the request
     */
    std::string request_method;

public:

    /**
     * Constructs a new response object for a particular request
     *
     * @param http_request_ptr the request that this is responding to
     */
    http_response(const http_request& http_request_ptr) :
    status_code(RESPONSE_CODE_OK),
    status_message(RESPONSE_MESSAGE_OK) {
        update_request_info(http_request_ptr);
    }

    /**
     * Copy constructor
     * 
     * @param http_response another instance
     */
    http_response(const http_response& http_response) :
    http_message(http_response),
    status_code(http_response.status_code),
    status_message(http_response.status_message),
    request_method(http_response.request_method) { }

    /**
     * Virtual destructor
     */
    virtual ~http_response() STATICLIB_NOEXCEPT { }

    /**
     * Clears all response data
     */
    virtual void clear() {
        http_message::clear();
        status_code = RESPONSE_CODE_OK;
        status_message = RESPONSE_MESSAGE_OK;
        request_method.clear();
    }

    /**
     * The content length may be implied for certain types of responses
     * 
     * @return whether content length implied
     */
    virtual bool is_content_length_implied() const {
        return (request_method == REQUEST_METHOD_HEAD // HEAD responses have no content
                || (status_code >= 100 && status_code <= 199) // 1xx responses have no content
                || status_code == 204 || status_code == 205 // no content & reset content responses
                || status_code == 304 // not modified responses have no content
                );
    }

    /**
     * Updates HTTP request information for the response object (use 
     * this if the response cannot be constructed using the request)
     *
     * @param http_request the request that this is responding to
     */
    void update_request_info(const http_request& http_request) {
        request_method = http_request.get_method();
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

    /**
     * Sets the HTTP response status code
     * 
     * @param n HTTP response status code
     */
    void set_status_code(unsigned int n) {
        status_code = n;
        clear_first_line();
    }

    /**
     * Sets the HTTP response status message
     * 
     * @param msg HTTP response status message
     */
    void set_status_message(const std::string& msg) {
        status_message = msg;
        clear_first_line();
    }

    /**
     * Returns the HTTP response status code
     * 
     * @return HTTP response status code
     */
    unsigned int get_status_code() const {
        return status_code;
    }

    /**
     * Returns the HTTP response status message
     * 
     * @return 
     */
    const std::string& get_status_message() const {
        return status_message;
    }

    /**
     * Returns `true` if this response can have a body
     * 
     * @return `true` if this response can have a body
     */
    bool is_body_allowed() const {
        return REQUEST_METHOD_HEAD != request_method;
    }

    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     * the cookie will be discarded by the user-agent when it closes
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     */
    void set_cookie(const std::string& name, const std::string& value) {
        std::string set_cookie_header(make_set_cookie_header(name, value, "/"));
        add_header(HEADER_SET_COOKIE, set_cookie_header);
    }

    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     * the cookie will be discarded by the user-agent when it closes
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param path the path of the cookie
     */
    void set_cookie(const std::string& name, const std::string& value, const std::string& path) {
        std::string set_cookie_header(make_set_cookie_header(name, value, path));
        add_header(HEADER_SET_COOKIE, set_cookie_header);
    }

    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param path the path of the cookie
     * @param max_age the life of the cookie, in seconds (0 = discard)
     */
    void set_cookie(const std::string& name, const std::string& value, const std::string& path, 
            const unsigned long max_age) {
        std::string set_cookie_header(make_set_cookie_header(name, value, path, true, max_age));
        add_header(HEADER_SET_COOKIE, set_cookie_header);
    }

    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param max_age the life of the cookie, in seconds (0 = discard)
     */
    void set_cookie(const std::string& name, const std::string& value, const unsigned long max_age) {
        std::string set_cookie_header(make_set_cookie_header(name, value, "/", true, max_age));
        add_header(HEADER_SET_COOKIE, set_cookie_header);
    }

    /**
     * Deletes cookie called name by adding a Set-Cookie header (cookie has no path)
     * 
     * @param name cookie name
     */
    void delete_cookie(const std::string& name) {
        std::string set_cookie_header(make_set_cookie_header(name, "", "/", true, 0));
        add_header(HEADER_SET_COOKIE, set_cookie_header);
    }
 
    /**
     * Deletes cookie called name by adding a Set-Cookie header (cookie has a path)
     * 
     * @param name cookie name
     * @param path cookie path
     */
    void delete_cookie(const std::string& name, const std::string& path) {
        std::string set_cookie_header(make_set_cookie_header(name, "", path, true, 0));
        add_header(HEADER_SET_COOKIE, set_cookie_header);
    }

    /**
     * Sets the time that the response was last modified (Last-Modified)
     * 
     * @param t last modified time
     */
    void set_last_modified(const unsigned long t);

protected:

    /**
     * Updates the string containing the first line for the HTTP message
     */
    virtual void update_first_line() const {
        // start out with the HTTP version
        m_first_line = get_version_string();
        m_first_line += ' ';
        // append the response status code
        m_first_line += sl::support::to_string(status_code);
        m_first_line += ' ';
        // append the response status message
        m_first_line += status_message;
    }

    /**
     * Appends HTTP headers for any cookies defined by the http::message
     */
    virtual void append_cookie_headers() {
        for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
                ::const_iterator i = get_cookies().begin(); i != get_cookies().end(); ++i) {
            set_cookie(i->first, i->second);
        }
    }

};

} // namespace
}

#endif // STATICLIB_PION_HTTP_RESPONSE_HPP
