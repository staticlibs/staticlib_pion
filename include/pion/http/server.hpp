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

#ifndef __PION_HTTP_SERVER_HEADER__
#define __PION_HTTP_SERVER_HEADER__

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <cstdint>

#include "asio.hpp"

#include "pion/config.hpp"
#include "pion/http/parser.hpp"
#include "pion/http/request.hpp"
#include "pion/tcp/connection.hpp"
#include "pion/tcp/server.hpp"


namespace pion {
namespace http {

/**
 * A server that handles HTTP connections
 */
class PION_API server : public tcp::server {

public:

    /**
     * Type of function that is used to handle requests
     */
    typedef std::function<void(http::request_ptr&, tcp::connection_ptr&) > request_handler_t;

    /**
     * Handler for requests that result in "500 Server Error"
     */
    typedef std::function<void(http::request_ptr&, tcp::connection_ptr&,
            const std::string&) > error_handler_t;

protected:
    
    /**
     * Maximum number of redirections
     */
    static const unsigned int MAX_REDIRECTS;

    /**
     * Data type for a map of resources to request handlers
     */
    typedef std::map<std::string, request_handler_t> resource_map_t;

    /**
     * Data type for a map of requested resources to other resources
     */
    typedef std::map<std::string, std::string> redirect_map_t;

    /**
     * Collection of resources that are recognized by this HTTP server
     */
    resource_map_t m_resources;

    /**
     * Collection of redirections from a requested resource to another resource
     */
    redirect_map_t m_redirects;

    /**
     * Points to a function that handles bad HTTP requests
     */
    request_handler_t m_bad_request_handler;

    /**
     * Points to a function that handles requests which match no web services
     */
    request_handler_t m_not_found_handler;

    /**
     * Points to the function that handles server errors
     */
    error_handler_t m_server_error_handler;

    /**
     * Mutex used to protect access to the resources
     */
    mutable std::mutex m_resource_mutex;

    /**
     * Maximum length for HTTP request payload content
     */
    std::size_t m_max_content_length;    
    
public:

    /**
     * Virtual destructor
     */
    virtual ~server() PION_NOEXCEPT;
    
    /**
     * Creates a new server object
     * 
     * @param tcp_port port number used to listen for new connections (IPv4)
     */
    explicit server(const unsigned int tcp_port = 0);

    /**
     * Creates a new server object
     * 
     * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
     */
    explicit server(const asio::ip::tcp::endpoint& endpoint);

    /**
     * Creates a new server object
     * 
     * @param sched the scheduler that will be used to manage worker threads
     * @param tcp_port port number used to listen for new connections (IPv4)
     */
    explicit server(scheduler& sched, const unsigned int tcp_port = 0);

    /**
     * Creates a new server object
     * 
     * @param sched the scheduler that will be used to manage worker threads
     * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
     */
    server(scheduler& sched, const asio::ip::tcp::endpoint& endpoint);

    /**
     * Adds a new web service to the HTTP server
     *
     * @param resource the resource name or uri-stem to bind to the handler
     * @param request_handler function used to handle requests to the resource
     */
    void add_resource(const std::string& resource, request_handler_t request_handler);

    /**
     * Removes a web service from the HTTP server
     *
     * @param resource the resource name or uri-stem to remove
     */
    void remove_resource(const std::string& resource);

    /**
     * Adds a new resource redirection to the HTTP server
     *
     * @param requested_resource the resource name or uri-stem that will be redirected
     * @param new_resource the resource that requested_resource will be redirected to
     */
    void add_redirect(const std::string& requested_resource, const std::string& new_resource);

    /**
     * Sets the function that handles bad HTTP requests
     * 
     * @param h function that handles bad HTTP requests
     */
    void set_bad_request_handler(request_handler_t h);

    /**
     * Sets the function that handles requests which match no other web services
     * 
     * @param h function that handles requests which match no other web services
     */
    void set_not_found_handler(request_handler_t h);

    /**
     * Sets the function that handles requests which match no other web services
     * 
     * @param h the function that handles requests which match no other web services
     */
    void set_error_handler(error_handler_t h);

    /**
     * Clears the collection of resources recognized by the HTTP server
     */
    virtual void clear();

    /**
     * Strips trailing slash from a string, if one exists
     *
     * @param str the original string
     * @return the resulting string, after any trailing slash is removed
     */
    static std::string strip_trailing_slash(const std::string& str);

    /**
     * Used to send responses when a bad HTTP request is made
     *
     * @param http_request_ptr the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
     */
    static void handle_bad_request(http::request_ptr& http_request_ptr, tcp::connection_ptr& tcp_conn);

    /**
     * Used to send responses when no web services can handle the request
     *
     * @param http_request_ptr the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
     */
    static void handle_not_found_request(http::request_ptr& http_request_ptr, 
            tcp::connection_ptr& tcp_conn);

    /**
     * Used to send responses when a server error occurs
     *
     * @param http_request_ptr the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
     * @param error_msg message that explains what went wrong
     */
    static void handle_server_error(http::request_ptr& http_request_ptr, 
            tcp::connection_ptr& tcp_conn, const std::string& error_msg);

    /**
     * Used to send responses when a request is forbidden
     *
     * @param http_request_ptr the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
     * @param error_msg message that explains what went wrong
     */
    static void handle_forbidden_request(http::request_ptr& http_request_ptr,
            tcp::connection_ptr& tcp_conn, const std::string& error_msg);

    /**
     * Used to send responses when a method is not allowed
     *
     * @param http_request_ptr the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
     * @param allowed_methods optional comma separated list of allowed methods
     */
    static void handle_method_not_allowed(http::request_ptr& http_request_ptr,
            tcp::connection_ptr& tcp_conn, const std::string& allowed_methods = "");

    /**
     * Sets the maximum length for HTTP request payload content
     * 
     * @param n maximum length for HTTP request payload content
     */
    inline void set_max_content_length(std::size_t n);

protected:

    /**
     * Handles a new TCP connection
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp::connection_ptr& tcp_conn);

    /**
     * Handles a new HTTP request
     *
     * @param http_request_ptr the HTTP request to handle
     * @param tcp_conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     */
    virtual void handle_request(http::request_ptr http_request_ptr,
            tcp::connection_ptr& tcp_conn, const asio::error_code& ec);

    /**
     * Searches for the appropriate request handler to use for a given resource
     *
     * @param resource the name of the resource to search for
     * @param request_handler function that can handle requests for this resource
     */
    virtual bool find_request_handler(const std::string& resource, 
            request_handler_t& request_handler) const;

};

} // end namespace http
} // end namespace pion

#endif
