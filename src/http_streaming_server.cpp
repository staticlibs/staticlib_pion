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
#include <stdexcept>

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

void streaming_server::handle_request(http::request_ptr http_request_ptr,
        tcp::connection_ptr tcp_conn, const asio::error_code& ec) {
    if (ec || !http_request_ptr->is_valid()) {
        tcp_conn->set_lifecycle(tcp::connection::LIFECYCLE_CLOSE); // make sure it will get closed
        if (tcp_conn->is_open() && (ec.category() == http::parser::get_error_category())) {
            // HTTP parser error
            PION_LOG_INFO(m_logger, "Invalid HTTP request (" << ec.message() << ")");
            m_bad_request_handler(http_request_ptr, tcp_conn);
        } else {
            static const asio::error_code
            ERRCOND_CANCELED(asio::error::operation_aborted, asio::error::system_category),
                    ERRCOND_EOF(asio::error::eof, asio::error::misc_category);

            if (ec == ERRCOND_CANCELED || ec == ERRCOND_EOF) {
                // don't spam the log with common (non-)errors that happen during normal operation
                PION_LOG_DEBUG(m_logger, "Lost connection on port " << get_port() << " (" << ec.message() << ")");
            } else {
                PION_LOG_INFO(m_logger, "Lost connection on port " << get_port() << " (" << ec.message() << ")");
            }

            tcp_conn->finish();
        }
        return;
    }

    PION_LOG_DEBUG(m_logger, "Received a valid HTTP request");

    // strip off trailing slash if the request has one
    std::string resource_requested(strip_trailing_slash(http_request_ptr->get_resource()));

    // apply any redirection
    redirect_map_t::const_iterator it = m_redirects.find(resource_requested);
    unsigned int num_redirects = 0;
    while (it != m_redirects.end()) {
        if (++num_redirects > MAX_REDIRECTS) {
            PION_LOG_ERROR(m_logger, "Maximum number of redirects (server::MAX_REDIRECTS) exceeded for requested resource: " << http_request_ptr->get_original_resource());
            m_server_error_handler(http_request_ptr, tcp_conn, "Maximum number of redirects (server::MAX_REDIRECTS) exceeded for requested resource");
            return;
        }
        resource_requested = it->second;
        http_request_ptr->change_resource(resource_requested);
        it = m_redirects.find(resource_requested);
    }

    // search for a handler matching the resource requested
    request_handler_t request_handler;
    // only this line is changed comparing to parent method
    if (find_method_specific_request_handler(http_request_ptr->get_method(), resource_requested, request_handler) || 
            find_request_handler(resource_requested, request_handler)) {

        // try to handle the request
        try {
            request_handler(http_request_ptr, tcp_conn);
            PION_LOG_DEBUG(m_logger, "Found request handler for HTTP resource: "
                    << resource_requested);
            if (http_request_ptr->get_resource() != http_request_ptr->get_original_resource()) {
                PION_LOG_DEBUG(m_logger, "Original resource requested was: " << http_request_ptr->get_original_resource());
            }
        } catch (std::bad_alloc&) {
            // propagate memory errors (FATAL)
            throw;
        } catch (std::exception& e) {
            // recover gracefully from other exceptions thrown by request handlers
            PION_LOG_ERROR(m_logger, "HTTP request handler: " << e.what());
            m_server_error_handler(http_request_ptr, tcp_conn, e.what());
        }

    } else {

        // no web services found that could handle the request
        PION_LOG_INFO(m_logger, "No HTTP request handlers found for resource: "
                << resource_requested);
        if (http_request_ptr->get_resource() != http_request_ptr->get_original_resource()) {
            PION_LOG_DEBUG(m_logger, "Original resource requested was: " << http_request_ptr->get_original_resource());
        }
        m_not_found_handler(http_request_ptr, tcp_conn);
    }
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
    payload_handler_creator_type creator;
    if (find_method_specific_payload_handler(http_request_ptr->get_method(), resource_requested, creator)) {
        auto ha = creator(http_request_ptr);
        http_request_ptr->set_payload_handler(std::move(ha));
    } else { // ignore request body as no payload_handler found
        PION_LOG_INFO(m_logger, "No payload handlers found for resource: " << resource_requested);
        rc = true;
    }
}

void streaming_server::add_method_specific_resource(const std::string& method, 
        const std::string& resource, request_handler_t request_handler) {
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    const std::string clean_resource(strip_trailing_slash(resource));
    auto en = std::make_pair(clean_resource, request_handler);
    if (types::REQUEST_METHOD_GET == method) {
        m_get_resources.insert(en);
    } else if (types::REQUEST_METHOD_POST == method) {
        m_post_resources.insert(en);
    } else if (types::REQUEST_METHOD_PUT == method) {
        m_put_resources.insert(en);
    } else if (types::REQUEST_METHOD_DELETE == method) {
        m_delete_resources.insert(en);
    }
    PION_LOG_INFO(m_logger, "Added handler for HTTP resource: " << clean_resource 
            << ", method: " << method);
}

void streaming_server::remove_method_specific_resource(const std::string& method,
        const std::string& resource) {
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    const std::string clean_resource(strip_trailing_slash(resource));    
    if (types::REQUEST_METHOD_GET == method) {
        m_get_resources.erase(clean_resource);
    } else if (types::REQUEST_METHOD_POST == method) {
        m_post_resources.erase(clean_resource);
    } else if (types::REQUEST_METHOD_PUT == method) {
        m_put_resources.erase(clean_resource);
    } else if (types::REQUEST_METHOD_DELETE == method) {
        m_delete_resources.erase(clean_resource);
    }
    PION_LOG_INFO(m_logger, "Removed handler for HTTP resource: " << clean_resource
            << ", method: " << method);
}

bool streaming_server::find_request_handler_internal(const resource_map_t& map, const std::string& resource, 
        request_handler_t& request_handler) const {
    // first make sure that HTTP resources are registered
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    if (map.empty()) return false;

    // iterate through each resource entry that may match the resource
    resource_map_t::const_iterator i = map.upper_bound(resource);
    while (i != map.begin()) {
        --i;
        // check for a match if the first part of the strings match
        if (i->first.empty() || resource.compare(0, i->first.size(), i->first) == 0) {
            // only if the resource matches the plug-in's identifier
            // or if resource is followed first with a '/' character
            if (resource.size() == i->first.size() || resource[i->first.size()] == '/') {
                request_handler = i->second;
                return true;
            }
        }
    }

    return false;
}

bool streaming_server::find_payload_handler_internal(const payloads_map_type& map, const std::string& resource,
        payload_handler_creator_type& payload_handler) const {
    // first make sure that HTTP resources are registered
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    if (map.empty()) return false;

    // iterate through each resource entry that may match the resource
    payloads_map_type::const_iterator i = map.upper_bound(resource);
    while (i != map.begin()) {
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

bool streaming_server::find_method_specific_request_handler(const std::string& method,
        const std::string& resource, request_handler_t& request_handler) const {
    if (types::REQUEST_METHOD_GET == method) {
        return find_request_handler_internal(m_get_resources, resource, request_handler);
    } else if (types::REQUEST_METHOD_POST == method) {
        return find_request_handler_internal(m_post_resources, resource, request_handler);
    } else if (types::REQUEST_METHOD_PUT == method) {
        return find_request_handler_internal(m_put_resources, resource, request_handler);
    } else if (types::REQUEST_METHOD_DELETE == method) {
        return find_request_handler_internal(m_delete_resources, resource, request_handler);
    } else return false;
}

void streaming_server::add_method_specific_payload_handler(const std::string& method,
        const std::string& resource, payload_handler_creator_type payload_handler) {
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    const std::string clean_resource(strip_trailing_slash(resource));
    if (types::REQUEST_METHOD_GET == method) {
        m_get_payloads.insert(std::make_pair(clean_resource, payload_handler));
    } else if (types::REQUEST_METHOD_POST == method) {
        m_post_payloads.insert(std::make_pair(clean_resource, payload_handler));
    } else if (types::REQUEST_METHOD_PUT == method) {
        m_put_payloads.insert(std::make_pair(clean_resource, payload_handler));
    } else if (types::REQUEST_METHOD_DELETE == method) {
        m_delete_payloads.insert(std::make_pair(clean_resource, payload_handler));        
    } else {
        throw std::runtime_error("Invalid payload method: [" + method + "]");
    }    
    PION_LOG_INFO(m_logger, "Added payload handler for HTTP resource: " << clean_resource
            << ", method: " << method);
}

void streaming_server::remove_method_specific_payload_handler(const std::string& method, 
        const std::string& resource) {
    std::unique_lock<std::mutex> resource_lock(m_resource_mutex, std::try_to_lock);
    const std::string clean_resource(strip_trailing_slash(resource));
    if (types::REQUEST_METHOD_GET == method) {
        m_get_payloads.erase(clean_resource);
    } else if (types::REQUEST_METHOD_POST == method) {
        m_post_payloads.erase(clean_resource);
    } else if (types::REQUEST_METHOD_PUT == method) {
        m_put_payloads.erase(clean_resource);
    } else if (types::REQUEST_METHOD_DELETE == method) {
        m_delete_payloads.erase(clean_resource);
    } else {
        throw std::runtime_error("Invalid payload method: [" + method + "]");
    }    
    PION_LOG_INFO(m_logger, "Removed payload handler for HTTP resource: " << clean_resource
            << ", method: " << method);
}

bool streaming_server::find_method_specific_payload_handler(const std::string& method,
        const std::string& resource, payload_handler_creator_type& payload_handler) const {
    if (types::REQUEST_METHOD_GET == method) {
        return find_payload_handler_internal(m_get_payloads, resource, payload_handler);
    } else if (types::REQUEST_METHOD_POST == method) {
        return find_payload_handler_internal(m_post_payloads, resource, payload_handler);
    } else if (types::REQUEST_METHOD_PUT == method) {
        return find_payload_handler_internal(m_put_payloads, resource, payload_handler);
    } else if (types::REQUEST_METHOD_DELETE == method) {
        return find_payload_handler_internal(m_delete_payloads, resource, payload_handler);
    } else return false;
}

} // end namespace http
} // end namespace pion
