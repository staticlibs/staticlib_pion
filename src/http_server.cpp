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
 * File:   http_server.cpp
 * Author: alex
 * 
 * Created on March 23, 2015, 8:19 PM
 */

#include "staticlib/pion/http_server.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>

#include "staticlib/pion/http_request_reader.hpp"
#include "staticlib/pion/pion_exception.hpp"
#include "staticlib/pion/http_response_writer.hpp"

#ifdef STATICLIB_PION_USE_SSL
#include "openssl/ssl.h"
#endif

namespace staticlib { 
namespace pion {

namespace { // anonymous

std::string strip_trailing_slash(const std::string& str) {
    std::string result{str};
    if (!result.empty() && '/' == result[result.size() - 1]) {
        result.resize(result.size() - 1);
    }
    return result;
}

template<typename T>
T& choose_map_by_method(const std::string& method, T& get_map, T& post_map, T& put_map, 
        T& delete_map, T& options_map) {
    if (http_message::REQUEST_METHOD_GET == method || http_message::REQUEST_METHOD_HEAD == method) {
        return get_map;
    } else if (http_message::REQUEST_METHOD_POST == method) {
        return post_map;
    } else if (http_message::REQUEST_METHOD_PUT == method) {
        return put_map;
    } else if (http_message::REQUEST_METHOD_DELETE == method) {
        return delete_map;
    } else if (http_message::REQUEST_METHOD_OPTIONS == method) {
        return options_map;
    } else {
        throw pion_exception("Invalid HTTP method: [" + method + "]");
    } 
}

void handle_bad_request(http_request_ptr& request, tcp_connection_ptr& conn) {
    static const std::string BAD_REQUEST_MSG = R"({
    "code": 400,
    "message": "Bad Request",
    "description": "Your browser sent a request that this server could not understand."
})";
    http_response_writer_ptr writer{http_response_writer::create(conn, request)};
    writer->get_response().set_status_code(http_message::RESPONSE_CODE_BAD_REQUEST);
    writer->get_response().set_status_message(http_message::RESPONSE_MESSAGE_BAD_REQUEST);
    writer->write_no_copy(BAD_REQUEST_MSG);
    writer->send();
}

void handle_not_found_request(http_request_ptr& request, tcp_connection_ptr& conn) {
    static const std::string NOT_FOUND_MSG_START = R"({
    "code": 404,
    "message": "Not Found",
    "description": "The requested URL: [)";
    static const std::string NOT_FOUND_MSG_FINISH = R"(] was not found on this server."
})";
    http_response_writer_ptr writer{http_response_writer::create(conn, request)};
    writer->get_response().set_status_code(http_message::RESPONSE_CODE_NOT_FOUND);
    writer->get_response().set_status_message(http_message::RESPONSE_MESSAGE_NOT_FOUND);
    writer->write_no_copy(NOT_FOUND_MSG_START);
    auto res = request->get_resource();
    std::replace(res.begin(), res.end(), '"', '\'');
    writer->write(res);
    writer->write_no_copy(NOT_FOUND_MSG_FINISH);
    writer->send();
}

void handle_server_error(http_request_ptr& request, tcp_connection_ptr& tcp_conn,
        const std::string& error_msg) {
    static const std::string SERVER_ERROR_MSG_START = R"({
    "code": 500,
    "message": "Internal Server Error",
    "description": ")";
    static const std::string SERVER_ERROR_MSG_FINISH = R"("
})";
    http_response_writer_ptr writer(http_response_writer::create(tcp_conn, request));
    writer->get_response().set_status_code(http_message::RESPONSE_CODE_SERVER_ERROR);
    writer->get_response().set_status_message(http_message::RESPONSE_MESSAGE_SERVER_ERROR);
    writer->write_no_copy(SERVER_ERROR_MSG_START);
    auto err = std::string(error_msg.data(), error_msg.length());
    std::replace(err.begin(), err.end(), '"', '\'');
    writer->write(err);
    writer->write_no_copy(SERVER_ERROR_MSG_FINISH);
    writer->send();
}

void handle_root_options(http_request_ptr& request, tcp_connection_ptr& conn) {
    auto writer = http_response_writer::create(conn, request);
    writer->get_response().change_header("Allow", "HEAD, GET, POST, PUT, DELETE, OPTIONS");
    writer->send();
}

void handle_100_continue(http_request_ptr& request, tcp_connection_ptr& conn) {
    auto writer = http_response_writer::create(conn, request);
    writer->write_no_copy(http_message::RESPONSE_FULLMESSAGE_100_CONTINUE);
    writer->send();
}

template<typename T>
typename T::iterator find_submatch(T& map, const std::string& path) {
    std::string st{path};
    auto end = map.end();
    std::string::size_type slash_ind = st.length();
    do {
        st = st.substr(0, slash_ind);
        auto it = map.find(st);
        if (end != it) {
            return it;
        }
        slash_ind = st.find_last_of("/");
    } while(std::string::npos != slash_ind);
    return end;
}

} // namespace

http_server::~http_server() STATICLIB_NOEXCEPT { }

http_server::http_server(uint32_t number_of_threads, uint16_t port,
        asio::ip::address_v4 ip_address,
        const std::string& ssl_key_file,
        std::function<std::string(std::size_t, asio::ssl::context::password_purpose)> ssl_key_password_callback,
        const std::string& ssl_verify_file,
        std::function<bool(bool, asio::ssl::verify_context&)> ssl_verify_callback
) : 
tcp_server(asio::ip::tcp::endpoint(ip_address, port), number_of_threads),
bad_request_handler(handle_bad_request),
not_found_handler(handle_not_found_request),
server_error_handler(handle_server_error) {
    if (!ssl_key_file.empty()) {
        this->ssl_flag = true;
        this->ssl_context.set_options(asio::ssl::context::default_workarounds
                | asio::ssl::context::no_compression
                | asio::ssl::context::no_sslv2
                | asio::ssl::context::single_dh_use);
        this->ssl_context.set_password_callback(ssl_key_password_callback);
        this->ssl_context.use_certificate_file(ssl_key_file, asio::ssl::context::pem);
        this->ssl_context.use_private_key_file(ssl_key_file, asio::ssl::context::pem);
        if (!ssl_verify_file.empty()) {
            this->ssl_context.load_verify_file(ssl_verify_file);
            this->ssl_context.set_verify_callback(ssl_verify_callback);
            this->ssl_context.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
            // https://www.openssl.org/docs/manmaster/ssl/SSL_CTX_set_session_id_context.html#WARNINGS
//            SSL_CTX_set_session_cache_mode(m_ssl_context.native_handle(), SSL_SESS_CACHE_OFF);
            SSL_CTX_set_session_id_context(ssl_context.native_handle(), reinterpret_cast<const unsigned char*>("pion"), 4);
        }
    }
}

void http_server::add_handler(const std::string& method,
        const std::string& resource, request_handler_type request_handler) {
    handlers_map_type& map = choose_map_by_method(method, get_handlers, post_handlers, put_handlers, 
            delete_handlers, options_handlers);
    const std::string clean_resource{strip_trailing_slash(resource)};
    STATICLIB_PION_LOG_DEBUG(log, "Added handler for HTTP resource: [" << clean_resource << "], method: [" << method << "]");
    auto it = map.emplace(std::move(clean_resource), std::move(request_handler));
    if (!it.second) throw pion_exception("Invalid duplicate handler path: [" + clean_resource + "], method: [" + method + "]");
}

void http_server::set_bad_request_handler(request_handler_type handler) {
    bad_request_handler = std::move(handler);
}

void http_server::set_not_found_handler(request_handler_type handler) {
    not_found_handler = std::move(handler);
}

void http_server::set_error_handler(error_handler_type handler) {
    server_error_handler = std::move(handler);
}

void http_server::add_payload_handler(const std::string& method, const std::string& resource,
        payload_handler_creator_type payload_handler) {
    payloads_map_type& map = choose_map_by_method(method, get_payloads, post_payloads, put_payloads, 
            delete_payloads, options_payloads);
    const std::string clean_resource{strip_trailing_slash(resource)};
    STATICLIB_PION_LOG_DEBUG(log, "Added payload handler for HTTP resource: [" << clean_resource << "], method: [" << method << "]");
    auto it = map.emplace(std::move(clean_resource), std::move(payload_handler));
    if (!it.second) throw pion_exception("Invalid duplicate payload path: [" + clean_resource + "], method: [" + method + "]");
}

void http_server::handle_connection(tcp_connection_ptr& conn) {
    http_request_reader::finished_handler_type fh = [this] (http_request_ptr request, 
            tcp_connection_ptr& conn, const std::error_code& ec) {
        this->handle_request(request, conn, ec);
    };
    reader_ptr my_reader_ptr = http_request_reader::create(conn, std::move(fh));
    http_request_reader::headers_parsing_finished_handler_type hpfh = [this](http_request_ptr request,
            tcp_connection_ptr& conn, const std::error_code& ec, sl::support::tribool & rc) {
        this->handle_request_after_headers_parsed(request, conn, ec, rc);
    };
    my_reader_ptr->set_headers_parsed_callback(std::move(hpfh));
    my_reader_ptr->receive();
}

void http_server::handle_request_after_headers_parsed(http_request_ptr request,
        tcp_connection_ptr& conn, const std::error_code& ec, sl::support::tribool& rc) {
    if (ec || !rc) return;
    // http://stackoverflow.com/a/17390776/314015
    if ("100-continue" == request->get_header("Expect")) {
        handle_100_continue(request, conn);
        rc = true;
        return;
    }
    auto& method = request->get_method();
    payloads_map_type& map = choose_map_by_method(method, get_payloads, post_payloads, put_payloads, 
            delete_payloads, options_payloads);
    std::string path{strip_trailing_slash(request->get_resource())};
    auto it = find_submatch(map, path);
    if (map.end() != it) {
        auto ha = it->second(request);
        request->set_payload_handler(std::move(ha));
    } else {
        // let's not spam client about GET and DELETE unlikely payloads
        if (http_message::REQUEST_METHOD_GET != method && 
                http_message::REQUEST_METHOD_DELETE != method &&
                http_message::REQUEST_METHOD_HEAD != method &&
                http_message::REQUEST_METHOD_OPTIONS != method) {
            STATICLIB_PION_LOG_WARN(log, "No payload handlers found for resource: " << path);
        }
        // ignore request body as no payload_handler found
        rc = true;
    }
}

void http_server::handle_request(http_request_ptr request, tcp_connection_ptr& conn,
        const std::error_code& ec) {
    // handle error
    if (ec || !request->is_valid()) {
        conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // make sure it will get closed
        if (conn->is_open() && (ec.category() == http_parser::get_error_category())) {
            // HTTP parser error
            STATICLIB_PION_LOG_INFO(log, "Invalid HTTP request (" << ec.message() << ")");
            bad_request_handler(request, conn);
        } else {
            static const std::error_code
            ERRCOND_CANCELED(asio::error::operation_aborted, asio::error::system_category),
                    ERRCOND_EOF(asio::error::eof, asio::error::misc_category);
            if (ec == ERRCOND_CANCELED || ec == ERRCOND_EOF) {
                // don't spam the log with common (non-)errors that happen during normal operation
                STATICLIB_PION_LOG_DEBUG(log, "Lost connection on port " << tcp_endpoint.port() << " (" << ec.message() << ")");
            } else {
                STATICLIB_PION_LOG_INFO(log, "Lost connection on port " << tcp_endpoint.port() << " (" << ec.message() << ")");
            }
            conn->finish();
        }
        return;
    }
    // handle request
    STATICLIB_PION_LOG_DEBUG(log, "Received a valid HTTP request");
    std::string path{strip_trailing_slash(request->get_resource())};
    if (http_message::REQUEST_METHOD_OPTIONS == request->get_method() && ("*" == path || "/*" == path)) {
        handle_root_options(request, conn);
        return;
    }    
    auto& method = request->get_method();
    handlers_map_type& map = choose_map_by_method(method, get_handlers, post_handlers, put_handlers, 
            delete_handlers, options_handlers);    
    auto handlers_it = find_submatch(map, path);
    if (map.end() != handlers_it) {
        request_handler_type& handler = handlers_it->second;
        try {
            STATICLIB_PION_LOG_DEBUG(log, "Found request handler for HTTP resource: " << path);
            handler(request, conn);
        } catch (std::bad_alloc&) {
            // propagate memory errors (FATAL)
            throw;
        } catch (std::exception& e) {
            // recover gracefully from other exceptions thrown by request handlers
            STATICLIB_PION_LOG_ERROR(log, "HTTP request handler: " << e.what());
            server_error_handler(request, conn, e.what());
        }
    } else {
        STATICLIB_PION_LOG_INFO(log, "No HTTP request handlers found for resource: " << path);
        not_found_handler(request, conn);
    }    
}

} // namespace
}
