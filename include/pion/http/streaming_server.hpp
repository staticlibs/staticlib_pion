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
    // typedefs are bad, and "_t" are prohibited, but lets be consistent with pion
    /// type of function that is used to create payload handlers
    typedef std::function<void(http::request_ptr&)> payload_handler_creator_t;
    
    /// data type for a map of resources to request handlers
    typedef std::map<std::string, payload_handler_creator_t> payloads_map_t;
    payloads_map_t m_payloads;
    
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
     * adds a new payload_handler to the HTTP server
     *
     * @param resource the resource name or uri-stem to bind to the handler
     * @param payload_handler function used to handle payload for the request
     */
    void add_payload_handler(const std::string& resource, payload_handler_creator_t payload_handler);

    /**
     * removes a payload_handler from the HTTP server
     *
     * @param resource the resource name or uri-stem to remove
     */
    void remove_payload_handler(const std::string& resource);

    /**
     * searches for the appropriate payload handler to use for a given resource
     *
     * @param resource the name of the resource to search for
     * @param payload_handler function that can handle payload for this resource
     */
    bool find_payload_handler(const std::string& resource,
            payload_handler_creator_t& payload_handler_creator) const;

protected:
    /**
     * handles a new TCP connection
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp::connection_ptr& tcp_conn);

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
    
};

} // end namespace http
} // end namespace pion

#endif	/* __PION_HTTP_STREAMING_SERVER_HPP__ */

