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

#include <pion/tribool.hpp>
#include <pion/tcp/connection.hpp>
#include <pion/http/request.hpp>
#include <pion/http/parser.hpp>
#include <pion/http/server.hpp>

namespace pion { // begin namespace pion
namespace http { // begin namespace http

class streaming_server : public server {   
       
protected:
    /// type of function that is used to create payload handlers
    typedef std::function<parser::payload_handler_t(http::request_ptr&)> payload_handler_creator_type;

    /// data type for a map of resources to request handlers
    typedef std::map<std::string, payload_handler_creator_type> payloads_map_type;    
    
public:
    /**
     * creates a new server object
     * 
     * @param number_of_threads number of threads to use for requests processing
     * @param port TCP port
     * @param ip_address IPv4-address to use, ANY address by default
     */
    explicit streaming_server(uint32_t number_of_threads, uint16_t port,
            asio::ip::address_v4 ip_address = asio::ip::address_v4::any())
    : server(asio::ip::tcp::endpoint(ip_address, port)) {
        get_active_scheduler().set_num_threads(number_of_threads);
    }
        
    /**
     * adds a new web service to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param request_handler function used to handle requests to the resource
     */
    void add_method_specific_resource(const std::string& method, const std::string& resource, request_handler_t request_handler);

    /**
     * removes a web service from the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to remove
     */
    void remove_method_specific_resource(const std::string& method, const std::string& resource);
    
    /**
     * adds a new payload_handler to the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to bind to the handler
     * @param payload_handler function used to handle payload for the request
     */
    void add_method_specific_payload_handler(const std::string& method, const std::string& resource, 
            payload_handler_creator_type payload_handler);

    /**
     * removes a payload_handler from the HTTP server
     *
     * @param method HTTP method name
     * @param resource the resource name or uri-stem to remove
     */
    void remove_method_specific_payload_handler(const std::string& method, const std::string& resource);

protected:

    /// collection of method specific resources that are recognized by this HTTP server
    resource_map_t m_get_resources;
    resource_map_t m_post_resources;
    resource_map_t m_put_resources;
    resource_map_t m_delete_resources;

    // map of resources to request handlers
    // GET payload won't happen, but lets be consistent here
    // to simplify clients implementation and to make sure that
    // pion's form-data parsing won't be triggered
    payloads_map_type m_get_payloads;
    payloads_map_type m_post_payloads;
    payloads_map_type m_put_payloads;
    payloads_map_type m_delete_payloads;
    
    /**
     * searches for the appropriate request handler to use for a given resource
     *
     * @param method the name of the HTTP method for specified resource to search for
     * @param resource the name of the resource to search for
     * @param request_handler function that can handle requests for this resource
     */
    virtual bool find_method_specific_request_handler(const std::string& method,
            const std::string& resource, request_handler_t& request_handler) const;
    
    /**
     * searches for the appropriate payload handler to use for a given resource
     *
     * @param resource the name of the resource to search for
     * @param payload_handler function that can handle payload for this resource
     */
    virtual bool find_method_specific_payload_handler(const std::string& method, 
            const std::string& resource, payload_handler_creator_type& payload_handler_creator) const;
    
    /**
     * handles a new TCP connection
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp::connection_ptr& tcp_conn) override;
    
    /**
     * handles a new HTTP request
     *
     * @param http_request_ptr the HTTP request to handle
     * @param tcp_conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     * @param rc parsing result code, false: abort, true: ignore_body, indeterminate: continue
     */
    virtual void handle_request_after_headers_parsed(http::request_ptr http_request_ptr,
            tcp::connection_ptr tcp_conn, const asio::error_code& ec, pion::tribool& rc);

    /**
     * handles a new HTTP request
     *
     * @param http_request_ptr the HTTP request to handle
     * @param tcp_conn TCP connection containing a new request
     * @param ec error_code contains additional information for parsing errors
     */
    virtual void handle_request(http::request_ptr http_request_ptr,
            tcp::connection_ptr tcp_conn, const asio::error_code& ec) override;

private:

    bool find_request_handler_internal(const resource_map_t& map , const std::string& resource, 
            request_handler_t& request_handler) const;

    bool find_payload_handler_internal(const payloads_map_type& map, const std::string& resource,
            payload_handler_creator_type& payload_handler) const;

};    

} // end namespace http
} // end namespace pion

#endif	/* __PION_HTTP_STREAMING_SERVER_HPP__ */

