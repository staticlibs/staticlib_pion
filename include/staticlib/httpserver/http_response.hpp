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

#ifndef STATICLIB_HTTPSERVER_HTTP_RESPONSE_HPP
#define STATICLIB_HTTPSERVER_HTTP_RESPONSE_HPP

#include <memory>

#include "staticlib/config.hpp"

#include "staticlib/httpserver/algorithm.hpp"
#include "staticlib/httpserver/http_message.hpp"
#include "staticlib/httpserver/http_request.hpp"

namespace staticlib { 
namespace httpserver {
    
/**
 * Container for HTTP response information
 */
class http_response : public http_message {

    /**
     * The HTTP response status code
     */
    unsigned int m_status_code;

    /**
     * The HTTP response status message
     */
    std::string m_status_message;

    /**
     * HTTP method used by the request
     */
    std::string m_request_method;
    
public:

    /**
     * Constructs a new response object for a particular request
     *
     * @param http_request_ptr the request that this is responding to
     */
    http_response(const http_request& http_request_ptr);

    /**
     * Constructs a new response object for a particular request method
     *
     * @param request_method the method used by the HTTP request we are responding to
     */
    http_response(const std::string& request_method);
    
    /**
     * Copy constructor
     * 
     * @param http_response another instance
     */
    http_response(const http_response& http_response);
    
    /**
     * Default constructor: you are strongly encouraged to use one of the other
     * constructors, since response parsing is influenced by the request method
     */
    http_response();
    
    /**
     * Virtual destructor
     */
    virtual ~http_response();

    /**
     * Clears all response data
     */
    virtual void clear();

    /**
     * The content length may be implied for certain types of responses
     * 
     * @return whether content length implied
     */
    virtual bool is_content_length_implied() const;

    /**
     * Updates HTTP request information for the response object (use 
     * this if the response cannot be constructed using the request)
     *
     * @param http_request the request that this is responding to
     */
    void update_request_info(const http_request& http_request);
    
    /**
     * Sets the HTTP response status code
     * 
     * @param n HTTP response status code
     */
    void set_status_code(unsigned int n);

    /**
     * Sets the HTTP response status message
     * 
     * @param msg HTTP response status message
     */
    void set_status_message(const std::string& msg);
    
    /**
     * Returns the HTTP response status code
     * 
     * @return HTTP response status code
     */
    unsigned int get_status_code() const;
    
    /**
     * Returns the HTTP response status message
     * 
     * @return 
     */
    const std::string& get_status_message() const;
    
    /**
     * Returns `true` if this response can have a body
     * 
     * @return `true` if this response can have a body
     */
    bool is_body_allowed() const;

    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     * the cookie will be discarded by the user-agent when it closes
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     */
    void set_cookie(const std::string& name, const std::string& value);
    
    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     * the cookie will be discarded by the user-agent when it closes
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param path the path of the cookie
     */
    void set_cookie(const std::string& name, const std::string& value, const std::string& path);
    
    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param path the path of the cookie
     * @param max_age the life of the cookie, in seconds (0 = discard)
     */
    void set_cookie(const std::string& name, const std::string& value, const std::string& path, 
            const unsigned long max_age);
    
    /**
     * Sets a cookie by adding a Set-Cookie header (see RFC 2109)
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param max_age the life of the cookie, in seconds (0 = discard)
     */
    void set_cookie(const std::string& name, const std::string& value, const unsigned long max_age);
    
    /**
     * Deletes cookie called name by adding a Set-Cookie header (cookie has no path)
     * 
     * @param name cookie name
     */
    void delete_cookie(const std::string& name);
    
    /**
     * Deletes cookie called name by adding a Set-Cookie header (cookie has a path)
     * 
     * @param name cookie name
     * @param path cookie path
     */
    void delete_cookie(const std::string& name, const std::string& path);
    
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
    virtual void update_first_line() const;
    
    /**
     * Appends HTTP headers for any cookies defined by the http::message
     */
    virtual void append_cookie_headers();

};

} // namespace
}

#endif // STATICLIB_HTTPSERVER_HTTP_RESPONSE_HPP
