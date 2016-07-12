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
 * File:   https_test.cpp
 * Author: alex
 *
 * Created on July 12, 2015, 5:17 PM
 */

#include <iostream>

#include <chrono>
#include <memory>
#include <thread>

#include "asio.hpp"

#include "staticlib/httpserver/tcp_connection.hpp"
#include "staticlib/httpserver/http_request.hpp"
#include "staticlib/httpserver/http_response_writer.hpp"
#include "staticlib/httpserver/http_server.hpp"

namespace sh = staticlib::httpserver;

#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8443;
#endif // STATICLIB_HTTPSERVER_HAVE_SSL

void test_https() {
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
    auto certpath = "../test/certificates/server/localhost.pem";
    auto pwdcb = [](std::size_t, asio::ssl::context::password_purpose) {
        return "test";
    };
    auto capath = "../test/certificates/server/staticlibs_test_ca.cer";
    auto verifier = [](bool, asio::ssl::verify_context&) {
        return true;
    };
    sh::http_server server(2, TCP_PORT, asio::ip::address_v4::any(), certpath, pwdcb, capath, verifier);
    server.add_handler("GET", "/", 
            [] (sh::http_request_ptr& http_request_ptr, sh::tcp_connection_ptr& tcp_conn) {
                auto writer = sh::http_response_writer::create(tcp_conn, http_request_ptr);
                writer << "Hello pion\n";
                writer->send();
            });
    server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    server.stop(true);
#endif // STATICLIB_HTTPSERVER_HAVE_SSL
}

int main() {
    try {
        test_https();
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}

