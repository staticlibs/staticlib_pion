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
#include <tuple>

#include "staticlib/pion/http_request_reader.hpp"
#include "staticlib/pion/http_response_writer.hpp"
#include "staticlib/pion/pion_exception.hpp"

#ifdef STATICLIB_PION_USE_SSL
#include "openssl/ssl.h"
#endif

namespace staticlib { 
namespace pion {

namespace { // anonymous

const std::string log = "staticlib.pion.http_server";

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

http_server::websocket_map_type& choose_ws_map(const std::string& event,
        http_server::websocket_map_type& open_map,
        http_server::websocket_map_type& message_map,
        http_server::websocket_map_type& close_map) {
    if ("WSOPEN" == event) {
        return open_map;
    } else if ("WSMESSAGE" == event) {
        return message_map;
    } else if ("WSCLOSE" == event) {
        return close_map;
    } else {
        throw pion_exception("Invalid WebSocket event: [" + event + "]");
    } 
}

void handle_bad_request(http_request_ptr, response_writer_ptr resp) {
    static const std::string BAD_REQUEST_MSG = R"({
    "code": 400,
    "message": "Bad Request",
    "description": "Your browser sent a request that this server could not understand."
})";
    resp->get_response().set_status_code(http_message::RESPONSE_CODE_BAD_REQUEST);
    resp->get_response().set_status_message(http_message::RESPONSE_MESSAGE_BAD_REQUEST);
    resp->write_nocopy(BAD_REQUEST_MSG);
    resp->send(std::move(resp));
}

void handle_not_found_request(http_request_ptr request, response_writer_ptr resp) {
    static const std::string NOT_FOUND_MSG_START = R"({
    "code": 404,
    "message": "Not Found",
    "description": "The requested URL: [)";
    static const std::string NOT_FOUND_MSG_FINISH = R"(] was not found on this server."
})";
    resp->get_response().set_status_code(http_message::RESPONSE_CODE_NOT_FOUND);
    resp->get_response().set_status_message(http_message::RESPONSE_MESSAGE_NOT_FOUND);
    resp->write_nocopy(NOT_FOUND_MSG_START);
    auto res = request->get_resource();
    std::replace(res.begin(), res.end(), '"', '\'');
    resp->write(res);
    resp->write_nocopy(NOT_FOUND_MSG_FINISH);
    resp->send(std::move(resp));
}

void handle_root_options(http_request_ptr, response_writer_ptr resp) {
    resp->get_response().change_header("Allow", "HEAD, GET, POST, PUT, DELETE, OPTIONS");
    resp->send(std::move(resp));
}

template<typename T>
typename T::iterator find_submatch(T& map, const std::string& path) {
    std::string st{path};
    auto end = map.end();
    std::string::size_type slash_ind = st.length();
    do {
        // todo: fixme
        st = st.substr(0, slash_ind);
        auto it = map.find(st);
        if (end != it) {
            return it;
        }
        slash_ind = st.find_last_of("/");
    } while(std::string::npos != slash_ind);
    return end;
}

std::tuple<bool, websocket_handler_type, websocket_handler_type, websocket_handler_type>
find_ws_handlers(const std::string& path, http_server::websocket_map_type& open_map,
        http_server::websocket_map_type& message_map, http_server::websocket_map_type& close_map) {
    auto it_open = open_map.find(path);
    auto has_open = open_map.end() != it_open;
    auto it_message = message_map.find(path);
    auto has_message = message_map.end() != it_message;
    auto it_close = close_map.find(path);
    auto has_close = close_map.end() != it_close;
    auto receive_fun = [](websocket_ptr ws) {
        websocket::receive(std::move(ws));
    };
    auto noop_fun = [](websocket_ptr) { };
    return std::make_tuple(
            has_open || has_message || has_close,
            has_open ? it_open->second : receive_fun,
            has_message ? it_message->second : receive_fun,
            has_close ? it_close->second : noop_fun);
}

void register_ws_conn(http_server::websocket_conn_registry_type& registry, std::mutex& mtx,
        const std::string& path, const std::string& id, std::weak_ptr<tcp_connection>& conn) {
    std::lock_guard<std::mutex> guard{mtx};
    // purge closed connections, this may take some time, done only on ws connect
    for (http_server::websocket_conn_registry_type::iterator it = registry.begin(); it != registry.end(); /* erase */) {
        if (it->second.second.expired()) {
            registry.erase(it++);
        } else {
            ++it;
        }
    }
    // insert new element
    registry.insert(std::make_pair(path, std::make_pair(id, conn)));
}

std::vector<std::shared_ptr<tcp_connection>> find_ws_conns(
        http_server::websocket_conn_registry_type& registry, std::mutex& mtx,
        const std::string& path, const std::set<std::string>& dest_ids) {
    std::lock_guard<std::mutex> guard{mtx};
    auto vec = std::vector<std::shared_ptr<tcp_connection>>();
    auto range = registry.equal_range(path);
    for (http_server::websocket_conn_registry_type::iterator it = range.first; it != range.second; /* erase */) {
        if (0 == dest_ids.size() || dest_ids.count(it->second.first) > 0) {
            auto shared = it->second.second.lock();
            if (nullptr != shared.get()) {
                vec.emplace_back(std::move(shared));
                ++it;
            } else {
                registry.erase(it++);
            }
        }
    }
    return vec;
}

} // namespace

http_server::http_server(uint32_t number_of_threads, uint16_t port,
        asio::ip::address_v4 ip_address,
        uint32_t read_timeout_millis,
        const std::string& ssl_key_file,
        std::function<std::string(std::size_t, asio::ssl::context::password_purpose)> ssl_key_password_callback,
        const std::string& ssl_verify_file,
        std::function<bool(bool, asio::ssl::verify_context&)> ssl_verify_callback
) : 
tcp_server(asio::ip::tcp::endpoint(ip_address, port), number_of_threads),
read_timeout(read_timeout_millis),
bad_request_handler(handle_bad_request),
not_found_handler(handle_not_found_request) {
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
    auto clean_resource = strip_trailing_slash(resource);
    STATICLIB_PION_LOG_DEBUG(log, "Adding handler for HTTP resource: [" << clean_resource << "], method: [" << method << "]");
    auto it = map.emplace(clean_resource, std::move(request_handler));
    if (!it.second) throw pion_exception("Invalid duplicate handler path: [" + clean_resource + "], method: [" + method + "]");
}

void http_server::add_payload_handler(const std::string& method, const std::string& resource,
        payload_handler_creator_type payload_handler) {
    payloads_map_type& map = choose_map_by_method(method, get_payloads, post_payloads, put_payloads, 
            delete_payloads, options_payloads);
    auto clean_resource = strip_trailing_slash(resource);
    STATICLIB_PION_LOG_DEBUG(log, "Adding payload handler for HTTP resource: [" << clean_resource << "], method: [" << method << "]");
    auto it = map.emplace(clean_resource, std::move(payload_handler));
    if (!it.second) throw pion_exception("Invalid duplicate payload path: [" + clean_resource + "], method: [" + method + "]");
}

void http_server::add_websocket_handler(const std::string& event, const std::string& resource, 
        websocket_handler_type handler) {
    auto& map = choose_ws_map(event, wsopen_handlers, wsmessage_handlers, wsclose_handlers);
    auto clean_resource = strip_trailing_slash(resource);
    STATICLIB_PION_LOG_DEBUG(log, "Adding WebSocket handler for resource: [" << clean_resource << "], event: [" << event << "]");
    auto it = map.emplace(std::move(clean_resource), std::move(handler));
    if (!it.second) throw pion_exception("Invalid duplicate WebSocket path: [" + clean_resource + "], event: [" + event + "]");
}

void http_server::broadcast_websocket(const std::string& path, sl::io::span<const char> message,
            sl::websocket::frame_type frame_type, const std::set<std::string>& dest_ids) {
    auto conns = find_ws_conns(websocket_conn_registry, websocket_conn_registry_mtx, path, dest_ids);
    if (conns.size() > 0) {
        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "Broadcasting WebSocket message," <<
                " path: [" << path << "], clients count: [" << conns.size() << "]," <<
                " message length: [" << message.size() << "]");
        websocket::broadcast(conns, message, frame_type);
    }
}

void http_server::handle_connection(tcp_connection_ptr& conn) {
    auto reader = sl::support::make_unique<http_request_reader>(*this, conn, read_timeout);
    reader->receive(std::move(reader));
    // reader is consumed at this point
}

void http_server::handle_request_after_headers_parsed(http_request_ptr& request,
        tcp_connection_ptr& conn, const std::error_code& ec, sl::support::tribool& rc) {
    if (ec || !rc) return;
    // http://stackoverflow.com/a/17390776/314015
    if (sl::utils::iequals("100-continue", request->get_header("Expect"))) {
        conn->async_write(asio::buffer(http_message::RESPONSE_FULLMESSAGE_100_CONTINUE),
                [](const std::error_code&, std::size_t){ /* no-op */ });
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
        conn->set_lifecycle(tcp_connection::lifecycle::close); // make sure it will get closed
        if (conn->is_open() && (ec.category() == http_parser::get_error_category())) {
            // HTTP parser error
            STATICLIB_PION_LOG_INFO(log, "Invalid HTTP request (" << ec.message() << ")");
            auto writer = sl::support::make_unique<http_response_writer>(conn, *request);
            bad_request_handler(std::move(request), std::move(writer));
        } else {
            if (asio::error::operation_aborted == ec.value() || asio::error::eof == ec.value()) {
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

    // check websocket upgrade
    if (websocket::is_websocket_upgrade(*request)) {
        auto tup = find_ws_handlers(request->get_resource(), wsopen_handlers, wsmessage_handlers, wsclose_handlers);
        if (std::get<0>(tup)) {
            // collect details for registry
            auto path = request->get_resource();
            auto id = request->get_header("Sec-WebSocket-Key");
            auto weak_conn = std::weak_ptr<tcp_connection>(conn);
            // create and start ws instance
            auto ws = sl::support::make_unique<websocket>(std::move(request), std::move(conn),
                    std::move(std::get<1>(tup)), std::move(std::get<2>(tup)), std::move(std::get<3>(tup)));
            ws->start(std::move(ws));
            // register connection for broadcasting
            register_ws_conn(websocket_conn_registry, websocket_conn_registry_mtx, path, id, weak_conn);
        } else {
            STATICLIB_PION_LOG_INFO(log, "No WebSocket handlers found for resource: " << request->get_resource());
            auto writer = sl::support::make_unique<http_response_writer>(conn, *request);
            not_found_handler(std::move(request), std::move(writer));
        }
        return;
    }

    // handle HTTP request
    auto path = std::string(strip_trailing_slash(request->get_resource()));
    auto writer = sl::support::make_unique<http_response_writer>(conn, *request);
    if (http_message::REQUEST_METHOD_OPTIONS == request->get_method() && ("*" == path || "/*" == path)) {
        handle_root_options(std::move(request), std::move(writer));
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
            handler(std::move(request), std::move(writer));
        } catch (std::bad_alloc&) {
            // propagate memory errors (FATAL)
            throw;
        } catch (std::exception& e) {
            // log exception from handler, but do not try to notify client
            // because response is consumed by handler (and its state is indeterminate)
            STATICLIB_PION_LOG_ERROR(log, "HTTP request handler: " << e.what());
        }
    } else {
        STATICLIB_PION_LOG_INFO(log, "No HTTP request handlers found for resource: " << path);
        not_found_handler(std::move(request), std::move(writer));
    }    
}

} // namespace
}
