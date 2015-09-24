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
#include <algorithm>
#include <cstdint>

#include "pion/tribool.hpp"
#include "pion/http/request_reader.hpp"
#include "pion/http/httpserver_exception.hpp"
#include "pion/http/filter_chain.hpp"
#include "pion/http/response_writer.hpp"
#include "pion/http/streaming_server.hpp"

namespace pion {
namespace http {

namespace { // anonymous

std::string strip_trailing_slash(const std::string& str) {
    std::string result{str};
    if (!result.empty() && '/' == result[result.size() - 1]) {
        result.resize(result.size() - 1);
    }
    return result;
}

template<typename T>
T& choose_map_by_method(const std::string& method, T& get_map, T& post_map, T& put_map, T& delete_map) {
    if (message::REQUEST_METHOD_GET == method) {
        return get_map;
    } else if (message::REQUEST_METHOD_POST == method) {
        return post_map;
    } else if (message::REQUEST_METHOD_PUT == method) {
        return put_map;
    } else if (message::REQUEST_METHOD_DELETE == method) {
        return delete_map;
    } else {
        throw httpserver_exception("Invalid HTTP method: [" + method + "]");
    } 
}

void handle_bad_request(http::request_ptr& request, tcp::connection_ptr& conn) {
    static const std::string BAD_REQUEST_MSG = R"({
    "code": 400,
    "message": "Bad Request",
    "description": "Your browser sent a request that this server could not understand."
})";
    http::writer_ptr writer{http::response_writer::create(request, conn)};
    writer->get_response().set_status_code(http::message::RESPONSE_CODE_BAD_REQUEST);
    writer->get_response().set_status_message(http::message::RESPONSE_MESSAGE_BAD_REQUEST);
    writer->write_no_copy(BAD_REQUEST_MSG);
    writer->send();
}

void handle_not_found_request(http::request_ptr& request, tcp::connection_ptr& conn) {
    static const std::string NOT_FOUND_MSG_START = R"({
    "code": 404,
    "message": "Not Found",
    "description": "The requested URL: [)";
    static const std::string NOT_FOUND_MSG_FINISH = R"(] was not found on this server."
})";
    http::writer_ptr writer{http::response_writer::create(request, conn)};
    writer->get_response().set_status_code(http::message::RESPONSE_CODE_NOT_FOUND);
    writer->get_response().set_status_message(http::message::RESPONSE_MESSAGE_NOT_FOUND);
    writer->write_no_copy(NOT_FOUND_MSG_START);
    auto res = algorithm::xml_encode(request->get_resource());
    std::replace(res.begin(), res.end(), '"', '\'');
    writer->write_move(std::move(res));
    writer->write_no_copy(NOT_FOUND_MSG_FINISH);
    writer->send();
}

void handle_server_error(http::request_ptr& http_request_ptr, tcp::connection_ptr& tcp_conn,
        const std::string& error_msg) {
    static const std::string SERVER_ERROR_MSG_START = R"({
    "code": 404,
    "message": "Not Found",
    "description": ")";
    static const std::string SERVER_ERROR_MSG_FINISH = R"("
})";
    http::writer_ptr writer(http::response_writer::create(tcp_conn, *http_request_ptr,
            std::bind(&tcp::connection::finish, tcp_conn)));
    writer->get_response().set_status_code(http::message::RESPONSE_CODE_SERVER_ERROR);
    writer->get_response().set_status_message(http::message::RESPONSE_MESSAGE_SERVER_ERROR);
    writer->write_no_copy(SERVER_ERROR_MSG_START);
    auto res = algorithm::xml_encode(error_msg);
    std::replace(res.begin(), res.end(), '"', '\'');
    writer->write_move(std::move(res));
    writer->write_no_copy(SERVER_ERROR_MSG_FINISH);
    writer->send();
}

} // namespace

streaming_server::~streaming_server() PION_NOEXCEPT { }

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
tcp::server(asio::ip::tcp::endpoint(ip_address, port)),
bad_request_handler(handle_bad_request),
not_found_handler(handle_not_found_request),
server_error_handler(handle_server_error) {
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

void streaming_server::add_handler(const std::string& method,
        const std::string& resource, request_handler_type request_handler) {
    handlers_map_type& map = choose_map_by_method(method, get_handlers, post_handlers, put_handlers, delete_handlers);
    const std::string clean_resource{strip_trailing_slash(resource)};
    PION_LOG_INFO(m_logger, "Added handler for HTTP resource: " << clean_resource << ", method: " << method);
    map.emplace(std::move(clean_resource), std::move(request_handler));
}

void streaming_server::set_bad_request_handler(request_handler_type handler) {
    bad_request_handler = std::move(handler);
}

void streaming_server::set_not_found_handler(request_handler_type handler) {
    not_found_handler = std::move(handler);
}

void streaming_server::set_error_handler(error_handler_type handler) {
    server_error_handler = std::move(handler);
}

void streaming_server::add_payload_handler(const std::string& method, const std::string& resource,
        payload_handler_creator_type payload_handler) {
    payloads_map_type& map = choose_map_by_method(method, get_payloads, post_payloads, put_payloads, delete_payloads);
    const std::string clean_resource{strip_trailing_slash(resource)};
    PION_LOG_INFO(m_logger, "Added payload handler for HTTP resource: " << clean_resource << ", method: " << method);
    map.emplace(std::move(clean_resource), std::move(payload_handler));
}

void streaming_server::add_filter(const std::string& method, const std::string& resource,
        request_filter_type filter) {
    filter_map_type& map = choose_map_by_method(method, get_filters, post_filters, put_filters, delete_filters);
    const std::string clean_resource{strip_trailing_slash(resource)};
    PION_LOG_INFO(m_logger, "Added filter for HTTP resource: " << clean_resource << ", method: " << method);
    map.emplace_back(std::move(clean_resource), std::move(filter));
}

void streaming_server::handle_connection(tcp::connection_ptr& conn) {
    reader_ptr my_reader_ptr = request_reader::create(conn, std::bind(
            &streaming_server::handle_request, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    http::request_reader::headers_parsing_finished_handler_t fun = std::bind(
            &streaming_server::handle_request_after_headers_parsed, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    my_reader_ptr->set_headers_parsed_callback(fun);
    my_reader_ptr->receive();
}

void streaming_server::handle_request_after_headers_parsed(http::request_ptr request,
        tcp::connection_ptr& /* conn */, const asio::error_code& ec, pion::tribool& rc) {
    if (ec || !rc) return;
    auto& method = request->get_method();
    payloads_map_type& map = choose_map_by_method(method, get_payloads, post_payloads, put_payloads, delete_payloads);
    std::string path{strip_trailing_slash(request->get_resource())};
    auto it = map.find(path);
    if (map.end() != it) {
        auto ha = it->second(request);
        request->set_payload_handler(std::move(ha));
    } else {
        // let's not spam client about GET and DELETE unlikely payloads
        if (message::REQUEST_METHOD_GET != method && message::REQUEST_METHOD_DELETE != method) {
            PION_LOG_INFO(m_logger, "No payload handlers found for resource: " << path);
        }
        // ignore request body as no payload_handler found
        rc = true;
    }
}

void streaming_server::handle_request(http::request_ptr request, tcp::connection_ptr& conn,
        const asio::error_code& ec) {
    // handle error
    if (ec || !request->is_valid()) {
        conn->set_lifecycle(tcp::connection::LIFECYCLE_CLOSE); // make sure it will get closed
        if (conn->is_open() && (ec.category() == http::parser::get_error_category())) {
            // HTTP parser error
            PION_LOG_INFO(m_logger, "Invalid HTTP request (" << ec.message() << ")");
            bad_request_handler(request, conn);
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
            conn->finish();
        }
        return;
    }
    // handle request
    PION_LOG_DEBUG(m_logger, "Received a valid HTTP request");
    auto& method = request->get_method();
    handlers_map_type& map = choose_map_by_method(method, get_handlers, post_handlers, put_handlers, delete_handlers);
    std::string path{strip_trailing_slash(request->get_resource())};
    auto handles_it = map.find(path);
    if (map.end() != handles_it) {
        request_handler_type& handler = handles_it->second;
        try {
            PION_LOG_DEBUG(m_logger, "Found request handler for HTTP resource: " << path);
            filter_map_type& filter_map = choose_map_by_method(method, get_filters, post_filters, put_filters, delete_filters);
            std::vector<std::reference_wrapper<request_filter_type>> filters{};
            for (auto& en : filter_map) {
                if (0 == path.compare(0, en.first.length(), en.first)) {
                    filters.emplace_back(std::ref(en.second));
                }
            }
            filter_chain fc{std::move(filters), handler};
            fc.do_filter(request, conn);
        } catch (std::bad_alloc&) {
            // propagate memory errors (FATAL)
            throw;
        } catch (std::exception& e) {
            // recover gracefully from other exceptions thrown by request handlers
            PION_LOG_ERROR(m_logger, "HTTP request handler: " << e.what());
            server_error_handler(request, conn, e.what());
        }
    } else {
        PION_LOG_INFO(m_logger, "No HTTP request handlers found for resource: " << path);
        not_found_handler(request, conn);
    }    
}

} // end namespace http
} // end namespace pion
