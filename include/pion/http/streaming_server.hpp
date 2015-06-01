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

#ifndef __PION_HTTP_STREAMING_SERVER_HPP__
#define	__PION_HTTP_STREAMING_SERVER_HPP__

#include <map>
#include <cstdint>

#include "asio.hpp"

#include "pion/tribool.hpp"
#include "pion/tcp/connection.hpp"
#include "pion/http/request.hpp"
#include "pion/http/parser.hpp"
#include "pion/http/server.hpp"

namespace pion {
namespace http {

class streaming_server : public server {   
       
protected:
    /**
     * Type of function that is used to create payload handlers
     */
    typedef std::function<parser::payload_handler_t(http::request_ptr&)> payload_handler_creator_type;

    /**
     * Data type for a map of resources to request handlers
     */
    typedef std::map<std::string, payload_handler_creator_type> payloads_map_type;    

    /**
     * Collection of GET resources that are recognized by this HTTP server
     */
    resource_map_t m_get_resources;
    
    /**
     * Collection of POST resources that are recognized by this HTTP server
     */
    resource_map_t m_post_resources;

    /**
     * Collection of PUT resources that are recognized by this HTTP server
     */
    resource_map_t m_put_resources;

    /**
     * Collection of DELETE resources that are recognized by this HTTP server
     */    
    resource_map_t m_delete_resources;

    /**
     * Collection of payload handlers GET resources that are recognized by this HTTP server
     */
    // GET payload won't happen, but lets be consistent here
    // to simplify clients implementation and to make sure that
    // pion's form-data parsing won't be triggered
    payloads_map_type m_get_payloads;

    /**
     * Collection of payload handlers POST resources that are recognized by this HTTP server
     */
    payloads_map_type m_post_payloads;

    /**
     * Collection of payload handlers PUT resources that are recognized by this HTTP server
     */
    payloads_map_type m_put_payloads;

    /**
     * Collection of payload handlers DELETE resources that are recognized by this HTTP server
     */
    payloads_map_type m_delete_payloads;
    
public:
    /**
     * Creates a new server object
     * 
     * @param number_of_threads number of threads to use for requests processing
     * @param port TCP port
     * @param ip_address IPv4-address to use, ANY address by default
     */
    explicit streaming_server(uint32_t number_of_threads, uint16_t port,
            asio::ip::address_v4 ip_address = asio::ip::address_v4::any());
        
    /**
     * Adds a new web service to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param request_handler function used to handle requests to the resource
     */
    void add_method_specific_resource(const std::string& method, const std::string& resource,
            request_handler_t request_handler);

    /**
     * Removes a web service from the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to remove
     */
    void remove_method_specific_resource(const std::string& method, const std::string& resource);
    
    /**
     * Adds a new payload_handler to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param payload_handler function used to handle payload for the request
     */
    void add_method_specific_payload_handler(const std::string& method, const std::string& resource, 
            payload_handler_creator_type payload_handler);

    /**
     * Removes a payload_handler from the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to remove
     */
    void remove_method_specific_payload_handler(const std::string& method, const std::string& resource);

protected:
    
    /**
     * Searches for the appropriate request handler to use for a given resource
     *
     * @param method the name of the HTTP method for specified resource to search for
     * @param resource the name of the resource to search for
     * @param request_handler function that can handle requests for this resource
     */
    virtual bool find_method_specific_request_handler(const std::string& method,
            const std::string& resource, request_handler_t& request_handler) const;
    
    /**
     * Searches for the appropriate payload handler to use for a given resource
     *
     * @param resource the name of the resource to search for
     * @param payload_handler function that can handle payload for this resource
     */
    virtual bool find_method_specific_payload_handler(const std::string& method, 
            const std::string& resource, payload_handler_creator_type& payload_handler_creator) const;
    
    /**
     * Handles a new TCP connection
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp::connection_ptr& tcp_conn) override;
    
    /**
     * Handles a new HTTP request
     *
     * @param http_request_ptr the HTTP request to handle
     * @param tcp_conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     * @param rc parsing result code, false: abort, true: ignore_body, indeterminate: continue
     */
    virtual void handle_request_after_headers_parsed(http::request_ptr http_request_ptr,
            tcp::connection_ptr tcp_conn, const asio::error_code& ec, pion::tribool& rc);

    /**
     * Handles a new HTTP request
     *
     * @param http_request_ptr the HTTP request to handle
     * @param tcp_conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     */
    virtual void handle_request(http::request_ptr http_request_ptr,
            tcp::connection_ptr tcp_conn, const asio::error_code& ec) override;

private:

    /**
     * Internal method to find out request handler for the specified resource path
     * 
     * @param map handlers map
     * @param resource resource path
     * @param request_handler out handler parameter
     * @return whether handler was found
     */
    bool find_request_handler_internal(const resource_map_t& map, const std::string& resource, 
            request_handler_t& request_handler) const;

    /**
     * Internal method to find out payload handler for the specified resource path
     * 
     * @param map handlers map
     * @param resource resource path
     * @param payload_handler out handler parameter
     * @return whether handler was found
     */
    bool find_payload_handler_internal(const payloads_map_type& map, const std::string& resource,
            payload_handler_creator_type& payload_handler) const;

};    

} // end namespace http
} // end namespace pion

#endif	/* __PION_HTTP_STREAMING_SERVER_HPP__ */

