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
 * File:   streaming_server.cpp
 * Author: alex
 * 
 * Created on March 23, 2015, 8:19 PM
 */

#include <functional>
#include <stdexcept>
#include <cstdint>

#include "pion/tribool.hpp"
#include "pion/http/request_reader.hpp"
#include "pion/http/streaming_server.hpp"

namespace pion {
namespace http {

streaming_server::streaming_server(uint32_t number_of_threads, uint16_t port,
        asio::ip::address_v4 ip_address
#ifdef PION_HAVE_SSL        
        ,
        const std::string& ssl_key_file,
        std::function<std::string(std::size_t, asio::ssl::context::password_purpose)> ssl_key_password_callback,
        const std::string& ssl_verify_file,
        std::function<bool(bool, asio::ssl::verify_context&)> ssl_verify_callback
#endif // PION_HAVE_SSL
) : 
server(asio::ip::tcp::endpoint(ip_address, port)) {
    get_active_scheduler().set_num_threads(number_of_threads);
#ifdef PION_HAVE_SSL
    if (!ssl_key_file.empty()) {
        set_ssl_flag(true);
        this->m_ssl_context.set_options(asio::ssl::context::default_workarounds
                | asio::ssl::context::no_compression
                | asio::ssl::context::no_sslv2
                | asio::ssl::context::single_dh_use);
        this->m_ssl_context.set_password_callback(ssl_key_password_callback);
        this->m_ssl_context.use_certificate_file(ssl_key_file, asio::ssl::context::pem);
        this->m_ssl_context.use_private_key_file(ssl_key_file, asio::ssl::context::pem);
        if (!ssl_verify_file.empty()) {
            this->m_ssl_context.load_verify_file(ssl_verify_file);
            this->m_ssl_context.set_verify_callback(ssl_verify_callback);
            this->m_ssl_context.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);            
        }
    }
#endif // PION_HAVE_SSL
}

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
    auto& method = http_request_ptr->get_method();
    if (find_method_specific_payload_handler(method, resource_requested, creator)) {
        auto ha = creator(http_request_ptr);
        http_request_ptr->set_payload_handler(std::move(ha));
    } else { // ignore request body as no payload_handler found
        // let's not spam client about GET and DELETE unlikely payloads
        if (types::REQUEST_METHOD_GET != method && types::REQUEST_METHOD_DELETE != method) {
            PION_LOG_INFO(m_logger, "No payload handlers found for resource: " << resource_requested);
        }
        rc = true;
    }
}

void streaming_server::add_method_specific_resource(const std::string& method, 
        const std::string& resource, request_handler_t request_handler) {
    std::lock_guard<std::mutex> resource_lock(m_resource_mutex);
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
    std::lock_guard<std::mutex> resource_lock(m_resource_mutex);
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
    std::lock_guard<std::mutex> resource_lock(m_resource_mutex);
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
    std::lock_guard<std::mutex> resource_lock(m_resource_mutex);
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
    std::lock_guard<std::mutex> resource_lock(m_resource_mutex);
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
    std::lock_guard<std::mutex> resource_lock(m_resource_mutex);
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
