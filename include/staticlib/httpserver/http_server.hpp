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

/* 
 * File:   streaming_server.hpp
 * Author: alex
 *
 * Created on March 23, 2015, 8:10 PM
 */

#ifndef STATICLIB_HTTPSERVER_HTTP_SERVER_HPP
#define	STATICLIB_HTTPSERVER_HTTP_SERVER_HPP

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <utility>

#include "asio.hpp"

#include "staticlib/config.hpp"
#include "staticlib/support.hpp"

#include "staticlib/httpserver/http_parser.hpp"
#include "staticlib/httpserver/http_request.hpp"
#include "staticlib/httpserver/tcp_connection.hpp"
#include "staticlib/httpserver/tcp_server.hpp"

namespace staticlib { 
namespace httpserver {

// forward declaration
class http_filter_chain;

/**
 * Server extension that supports streaming requests of arbitrary size (file upload)
 */
class http_server : public tcp_server {   
    friend class http_filter_chain;   
    
protected:
    /**
     * Type of function that is used to handle requests
     */
    using request_handler_type = std::function<void(http_request_ptr&, tcp_connection_ptr&)>;

    /**
     * Handler for requests that result in "500 Server Error"
     */
    using error_handler_type = std::function<void(http_request_ptr&, tcp_connection_ptr&,
            const std::string&)> ;

    /**
     * Type of function that is used to create payload handlers
     */
    using payload_handler_creator_type = std::function<http_parser::payload_handler_type(http_request_ptr&)>;

    /**
     * Type for filters
     */
    using request_filter_type = std::function<void(http_request_ptr&, tcp_connection_ptr&, http_filter_chain&)>;
    
    /**
     * Data type for a map of resources to request handlers
     */
    using handlers_map_type = std::unordered_map<std::string, request_handler_type>;
    
    /**
     * Data type for a map of resources to request handlers
     */
    using payloads_map_type = std::unordered_map<std::string, payload_handler_creator_type>; 
    
    /**
     * Data type for a multi map of filters
     */
    using filter_map_type = std::unordered_multimap<std::string, request_filter_type, algorithm::ihash, algorithm::iequal_to>;
    
    /**
     * Collection of GET handlers that are recognized by this HTTP server
     */
    handlers_map_type get_handlers;

    /**
     * Collection of POST handlers that are recognized by this HTTP server
     */
    handlers_map_type post_handlers;

    /**
     * Collection of PUT handlers that are recognized by this HTTP server
     */
    handlers_map_type put_handlers;

    /**
     * Collection of DELETE handlers that are recognized by this HTTP server
     */    
    handlers_map_type delete_handlers;

    /**
     * Collection of OPTIONS handlers that are recognized by this HTTP server
     */
    handlers_map_type options_handlers;

    /**
     * Points to a function that handles bad HTTP requests
     */
    request_handler_type bad_request_handler;

    /**
     * Points to a function that handles requests which match no web services
     */
    request_handler_type not_found_handler;

    /**
     * Points to the function that handles server errors
     */
    error_handler_type server_error_handler;

    /**
     * Collection of payload handlers GET resources that are recognized by this HTTP server
     */
    // GET payload won't happen, but lets be consistent here
    // to simplify clients implementation and to make sure that
    // pion's form-data parsing won't be triggered
    payloads_map_type get_payloads;

    /**
     * Collection of payload handlers POST resources that are recognized by this HTTP server
     */
    payloads_map_type post_payloads;

    /**
     * Collection of payload handlers PUT resources that are recognized by this HTTP server
     */
    payloads_map_type put_payloads;

    /**
     * Collection of payload handlers DELETE resources that are recognized by this HTTP server
     */
    payloads_map_type delete_payloads;

    /**
     * Collection of payload handlers OPTIONS resources that are recognized by this HTTP server
     */
    payloads_map_type options_payloads;

    /**
     * Collection of GET filters
     */
    filter_map_type get_filters;

    /**
     * Collection of POST filters
     */
    filter_map_type post_filters;

    /**
     * Collection of PUT filters
     */
    filter_map_type put_filters;

    /**
     * Collection of DELETE filters
     */
    filter_map_type delete_filters;

    /**
     * Collection of OPTIONS filters
     */
    filter_map_type options_filters;

public:
    ~http_server() STATICLIB_NOEXCEPT;
    
    /**
     * Creates a new server object
     * 
     * @param number_of_threads number of threads to use for requests processing
     * @param port TCP port
     * @param ip_address (optional) IPv4-address to use, ANY address by default
     * @param ssl_key_file (optional) path file containing concatenated X.509 key and certificate,
     *        empty by default (SSL disabled)
     * @param ssl_key_password_callback (optional) callback function that should return a password
     *        that will be used to decrypt a PEM-encoded RSA private key file
     * @param ssl_verify_file (optional) path to file containing one or more CA certificates in PEM format
     * @param ssl_verify_callback (optional) callback function that can be used to customize client
     *        certificate auth
     */
    explicit http_server(uint32_t number_of_threads, uint16_t port,
            asio::ip::address_v4 ip_address = asio::ip::address_v4::any()
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
            ,
            const std::string& ssl_key_file = std::string(),
            std::function<std::string(std::size_t, asio::ssl::context::password_purpose)> ssl_key_password_callback = 
                    [](std::size_t, asio::ssl::context::password_purpose) { return std::string(); },
            const std::string& ssl_verify_file = std::string(),
            std::function<bool(bool, asio::ssl::verify_context&)> ssl_verify_callback = 
                    [](bool, asio::ssl::verify_context&) { return true; }
#endif // STATICLIB_HTTPSERVER_HAVE_SSL
            );
        
    /**
     * Adds a new web service to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param request_handler function used to handle requests to the resource
     */
    void add_handler(const std::string& method, const std::string& resource,
            request_handler_type request_handler);

    /**
     * Sets the function that handles bad HTTP requests
     * 
     * @param handler function that handles bad HTTP requests
     */
    void set_bad_request_handler(request_handler_type handler);

    /**
     * Sets the function that handles requests which match no other web services
     * 
     * @param handler function that handles requests which match no other web services
     */
    void set_not_found_handler(request_handler_type handler);

    /**
     * Sets the function that handles requests which match no other web services
     * 
     * @param h the function that handles requests which match no other web services
     */
    void set_error_handler(error_handler_type handler);
    
    /**
     * Adds a new payload_handler to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param payload_handler function used to handle payload for the request
     */
    void add_payload_handler(const std::string& method, const std::string& resource, 
            payload_handler_creator_type payload_handler);

    /**
     * Adds a new filter to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param request_handler filter function
     */    
    void add_filter(const std::string& method, const std::string& resource,
            request_filter_type filter);

protected:
    
    /**
     * Handles a new TCP connection
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp_connection_ptr& conn) override;
    
    /**
     * Handles a new HTTP request
     *
     * @param request the HTTP request to handle
     * @param conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     * @param rc parsing result code, false: abort, true: ignore_body, indeterminate: continue
     */
    virtual void handle_request_after_headers_parsed(http_request_ptr request,
            tcp_connection_ptr& conn, const asio::error_code& ec, sl::support::tribool& rc);

    /**
     * Handles a new HTTP request
     *
     * @param request the HTTP request to handle
     * @param conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     */
    virtual void handle_request(http_request_ptr request,
            tcp_connection_ptr& conn, const asio::error_code& ec);
   
    
};    

} // namespace
}

#endif	/* STATICLIB_HTTPSERVER_HTTP_SERVER_HPP */

