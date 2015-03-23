/* 
 * File:   streaming_server.cpp
 * Author: alex
 * 
 * Created on March 23, 2015, 8:19 PM
 */

#include <functional>
#include <cstdint>

#include <pion/tribool.hpp>
#include <pion/http/streaming_server.hpp>

#include "pion/http/request_reader.hpp"

namespace pion { // begin namespace pion
namespace http { // begin namespace http

void streaming_server::handle_connection(tcp::connection_ptr& tcp_conn) {
    request_reader_ptr my_reader_ptr;
    my_reader_ptr = request_reader::create(tcp_conn, std::bind(&streaming_server::handle_request, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    http::request_reader::headers_parsing_finished_handler_t fun = std::bind(
            &streaming_server::handle_request_after_headers_parsed, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    my_reader_ptr->set_headers_parsed_callback(fun);
    my_reader_ptr->receive();
}

void streaming_server::handle_request_after_headers_parsed(http::request_ptr http_request_ptr,
        tcp::connection_ptr /* tcp_conn */, const asio::error_code& ec, pion::tribool& rc) {
    if (ec || !rc) return;

    // strip off trailing slash if the request has one
    std::string resource_requested(strip_trailing_slash(http_request_ptr->get_resource()));

    // apply any redirection
    redirect_map_t::const_iterator it = m_redirects.find(resource_requested);
    unsigned int num_redirects = 0;
    while (it != m_redirects.end()) {
        if (++num_redirects > MAX_REDIRECTS) {
            return;
        }
        resource_requested = it->second;
        it = m_redirects.find(resource_requested);
    }

    // search for a handler matching the resource requested
    parser::payload_handler_t payload_handler;
    if (find_payload_handler(resource_requested, payload_handler)) {
        http_request_ptr->get_request_reader()->set_payload_handler(payload_handler);
        http_request_ptr->set_request_reader(NULL);
    } else { // ignore request body as no payload_handler found
        PION_LOG_INFO(m_logger, "No payload handlers found for resource: " << resource_requested);
        rc = true;
    }
}

void streaming_server::add_payload_handler(const std::string& resource,
        parser::payload_handler_t payload_handler) {
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    const std::string clean_resource(strip_trailing_slash(resource));
    m_payloads.insert(std::make_pair(clean_resource, payload_handler));
    PION_LOG_INFO(m_logger, "Added payload handler for HTTP resource: " << clean_resource);
}

void streaming_server::remove_payload_handler(const std::string& resource) {
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    const std::string clean_resource(strip_trailing_slash(resource));
    m_payloads.erase(clean_resource);
    PION_LOG_INFO(m_logger, "Removed payload handler for HTTP resource: " << clean_resource);
}

bool streaming_server::find_payload_handler(const std::string& resource,
        parser::payload_handler_t& payload_handler) const {
    // first make sure that HTTP resources are registered
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    if (m_payloads.empty())
        return false;

    // iterate through each resource entry that may match the resource
    payloads_map_t::const_iterator i = m_payloads.upper_bound(resource);
    while (i != m_payloads.begin()) {
        --i;
        // check for a match if the first part of the strings match
        if (i->first.empty() || resource.compare(0, i->first.size(), i->first) == 0) {
            // only if the resource matches the plug-in's identifier
            // or if resource is followed first with a '/' character
            if (resource.size() == i->first.size() || resource[i->first.size()] == '/') {
                payload_handler = i->second;
                return true;
            }
        }
    }

    return false;
}

} // end namespace http
} // end namespace pion
