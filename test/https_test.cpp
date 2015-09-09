/* 
 * File:   https_test.cpp
 * Author: alex
 *
 * Created on July 12, 2015, 5:17 PM
 */

#include <iostream>
#include <thread>
#include <chrono>

#include "asio.hpp"

#include <pion/tcp/connection.hpp>
#include <pion/http/request.hpp>
#include <pion/http/response_writer.hpp>
#include <pion/http/streaming_server.hpp>

#ifdef PION_HAVE_SSL
const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8443;
#endif // PION_HAVE_SSL

int main() {
#ifdef PION_HAVE_SSL
    auto certpath = "../certificates/server/localhost.pem";
    auto pwdcb = [](std::size_t, asio::ssl::context::password_purpose) {
        return "test";
    };
    auto capath = "../certificates/server/staticlibs_test_ca.cer";
    auto verifier = [](bool, asio::ssl::verify_context&) {
        return true;
    };
    pion::http::streaming_server server(2, TCP_PORT, asio::ip::address_v4::any(), certpath, pwdcb, capath, verifier);
    server.add_method_specific_resource("GET", "/", 
            [] (pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr & tcp_conn) {
                auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
                auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
                writer << "Hello pion\n";
                writer->send();
            });
    server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    server.stop(true);
#endif // PION_HAVE_SSL
    return 0;
}

