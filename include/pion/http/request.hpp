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

#ifndef __PION_HTTP_REQUEST_HEADER__
#define __PION_HTTP_REQUEST_HEADER__

#include <functional>
#include <string>

#include "pion/config.hpp"
#include "pion/http/message.hpp"
#include "pion/http/parser.hpp"


namespace pion {
namespace http {

/**
 * Container for HTTP request information
 */
class request : public http::message {

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
    parser::payload_handler_t m_payload_handler;

    /**
     * Non-owning pointer to request_reader to be used during parsing
     */
    parser* m_request_reader;    
    
public:

    /**
     * Constructs a new request object
     *
     * @param resource the HTTP resource to request
     */
    request(const std::string& resource);
    
    /**
     * Constructs a new request object (default constructor)
     */
    request();
    
    /**
     * Virtual destructor
     */
    virtual ~request();

    /**
     * Clears all request data
     */
    virtual void clear();

    /**
     * The content length of the message can never be implied for requests
     * 
     * @return false
     */
    virtual bool is_content_length_implied() const;

    /**
     * Returns the request method (i.e. GET, POST, PUT)
     * 
     * @return request method
     */
    const std::string& get_method() const;
    
    /**
     * Returns the resource uri-stem to be delivered (possibly the result of a redirect)
     * 
     * @return resource uri-stem to be delivered
     */
    const std::string& get_resource() const;

    /**
     * Returns the resource uri-stem originally requested
     * 
     * @return resource uri-stem originally requested
     */
    const std::string& get_original_resource() const;

    /**
     * Returns the uri-query or query string requested
     * 
     * @return uri-query or query string requested
     */
    const std::string& get_query_string() const;
    
    /**
     * Returns a value for the query key if any are defined; otherwise, an empty string
     * 
     * @param key query key
     * @return value for the query key if any are defined; otherwise, an empty string
     */
    const std::string& get_query(const std::string& key) const;

    /**
     * Returns the query parameters
     * 
     * @return query parameters multimap
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& get_queries();
    
    /**
     * Returns true if at least one value for the query key is defined
     * 
     * @param key query key
     * @return true if at least one value for the query key is defined
     */
    bool has_query(const std::string& key) const;
        
    /**
     * Sets the HTTP request method (i.e. GET, POST, PUT)
     * 
     * @param str request method
     */
    void set_method(const std::string& str);
    
    /**
     * Sets the resource or uri-stem originally requested
     */
    void set_resource(const std::string& str);

    /**
     * Changes the resource or uri-stem to be delivered (called as the result of a redirect)
     * 
     * @param str resource or uri-stem to be delivered
     */
    void change_resource(const std::string& str);

    /**
     * Sets the uri-query or query string requested
     * 
     * @param str uri-query or query string requested
     */
    void set_query_string(const std::string& str);
    
    /**
     * Adds a value for the query key
     * 
     * @param key query key
     * @param value additional value for this key
     */
    void add_query(const std::string& key, const std::string& value);
    
    /**
     * Changes the value of a query key
     * 
     * @param key query key
     * @param value value for this key
     */
    void change_query(const std::string& key, const std::string& value);
    
    /**
     * Removes all values for a query key
     * 
     * @param key query key
     */
    void delete_query(const std::string& key);
    
    /**
     * Use the query parameters to build a query string for the request
     */
    void use_query_params_for_query_string();

    /**
     * Use the query parameters to build POST content for the request
     */
    void use_query_params_for_post_content();

    /**
     * Add content (for POST) from string
     * 
     * @param value POST content
     */
    void set_content(const std::string &value);
    
    /**
     * Add content (for POST) from buffer of given size;
     * Does nothing if the buffer is invalid or the buffer size is zero
     */
    void set_content(const char* value, size_t size);

    /**
     * This method should be called from payload_handler_creator
     * to stick payload handler to this request
     * 
     * @param ph payload handler
     */
    void set_payload_handler(parser::payload_handler_t ph);
    
    /**
     * Access to the wrapped payload handler object
     * 
     * @return unwrapped payload handler object
     */
    template<typename T>
    T* get_payload_handler() { return m_payload_handler.target<T>(); }
    
    /**
     * Access to the payload handler wrapper
     * 
     * @return payload handler
     */
    parser::payload_handler_t& get_payload_handler_wrapper();
    
    /**
     * Internal method used with payload handlers
     */
    void set_request_reader(parser* rr);

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

/**
 * Data type for a HTTP request pointer
 */
typedef std::shared_ptr<request> request_ptr;


} // end namespace http
} // end namespace pion

#endif
