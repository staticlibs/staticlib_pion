/*
 * Copyright 2018, alex at staticlibs.net
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
 * File:   websocket_test.cpp
 * Author: alex
 *
 * Created on June 19, 2018, 10:45 AM
 */

#include <cstdint>
#include <array>
#include <chrono>
#include <iostream>
#include <thread>

#include "asio.hpp"

#include "staticlib/config/assert.hpp"
#include "staticlib/io.hpp"
#include "staticlib/support.hpp"
#include "staticlib/websocket.hpp"

#include "staticlib/pion/http_server.hpp"

const uint16_t TCP_PORT = 8080;

void wsopen(sl::pion::websocket_ptr ws) {
    std::cout << "open" << std::endl;
    ws->write("open");
    ws->send(std::move(ws));
}

void wsmsg(sl::pion::websocket_ptr ws) {
    auto src = ws->message_data();
    auto sink = sl::io::string_sink();
    sl::io::copy_all(src, sink);
    if ("closeme" == sink.get_string()) {
        ws->close(std::move(ws));
    } else {
        ws->write(sink.get_string());
        ws->send(std::move(ws));
    }
}

void wsclose(sl::pion::websocket_ptr ws) {
    (void) ws;
    std::cout << "closed" << std::endl;
}

std::string make_handshake() {
    auto res = sl::websocket::handshake::make_request_line("/mirror");
    auto vec = sl::websocket::handshake::make_request_headers(
            "127.0.0.1", TCP_PORT, sl::io::string_from_hex("0102030405060708090a0b0c0d0e0f10"));
    for (auto& el : vec) {
        res += el.first;
        res += ": ";
        res += el.second;
        res += "\r\n";
    }
    res += "\r\n";
    return res;
}

std::string make_frame(const std::string& msg, bool final = true, bool cont = false) {
    std::array<char, 10> hbuf;
    auto ftype = cont ? sl::websocket::frame_type::continuation : sl::websocket::frame_type::text;
    auto header = sl::websocket::frame::make_header(hbuf, ftype, msg.size(), true, !final);
    auto res = std::string(header.data(), header.size());
    auto mask = 0x12345678;
    char* mask_ptr = reinterpret_cast<char*>(std::addressof(mask));
    for (size_t i = 0; i < 4; i++) {
        res.push_back(mask_ptr[3-i]);
    }
    auto msg_masked = std::string();
    msg_masked.resize(msg.size());
    for (size_t i = 0; i < msg.size(); i++) {
        msg_masked[i] = msg[i] ^ mask_ptr[3 - i%4];
    }
    res += msg_masked;
    return res;
}

std::string receive(asio::ip::tcp::socket& socket) {
    auto buf = std::string();
    buf.resize(1024);
    auto buf_len = socket.receive(asio::buffer(std::addressof(buf.front()), buf.length()));
    buf.resize(buf_len);
    auto frame = sl::websocket::frame({buf.data(), buf.length()});
    return std::string(frame.payload().data(), frame.payload().size());
}

void check_hello(asio::ip::tcp::socket& socket) {
    auto msg = std::string("hello");
    auto hello = make_frame(msg);
    socket.send(asio::buffer(hello));
    slassert(msg == receive(socket));
}

void check_tcp_fragment_1_byte(asio::ip::tcp::socket& socket) {
    auto msg = std::string("hello");
    auto hello = make_frame(msg);
    for (size_t i = 0; i < hello.length(); i++) {
        socket.send(asio::buffer(hello.data() + i, 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    slassert(msg == receive(socket));
}

void check_tcp_fragment_split(asio::ip::tcp::socket& socket) {
    auto msg1 = std::string("hello");
    auto hello = make_frame(msg1);
    auto msg2 = std::string("bye");
    auto bye = make_frame(msg2);
    auto send_buf = hello + bye;
    socket.send(asio::buffer(send_buf.data(), send_buf.length() - 1));
    slassert(msg1 == receive(socket));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    socket.send(asio::buffer(send_buf.data() + (send_buf.length() - 1), 1));
    slassert(msg2 == receive(socket));
}

void check_continuation(asio::ip::tcp::socket& socket) {
    auto msg = std::string("hello");
    auto he = make_frame(msg.substr(0, 2), false, false);
    auto l = make_frame(msg.substr(2, 1), false, true);
    auto lo = make_frame(msg.substr(3, 2), true, true);
    auto send_buf = he + l + lo;
    socket.send(asio::buffer(send_buf));
    slassert(msg == receive(socket));
}

void check_broadcast(sl::pion::http_server& server, asio::ip::tcp::socket& socket) {
    auto msg = std::string("hello_broadcast");
    server.broadcast_websocket("/mirror", msg);
    slassert(msg == receive(socket));
}

void check_long(asio::ip::tcp::socket& socket) {
    auto msg = std::string();
    for(size_t i = 0; msg.size() < (1<<17); i++) {
        msg += sl::support::to_string(i);
    }
    auto numbers = make_frame(msg);
    socket.send(asio::buffer(numbers));
    auto buf = std::string();
    buf.resize(numbers.size() - 4);
    size_t received = 0;
    while (received < buf.length()) {
        received += socket.receive(asio::buffer(std::addressof(buf.front()) + received, buf.length() - received));
    }
    auto frame = sl::websocket::frame({buf.data(), buf.length()});
    auto retmsg = std::string(frame.payload().data(), frame.payload().size());
    slassert(msg == retmsg);
}

void check_close(asio::ip::tcp::socket& socket) {
    auto msg = std::string("closeme");
    auto hello = make_frame(msg);
    socket.send(asio::buffer(hello));
    auto resp = receive(socket);
    slassert("03e8" == sl::io::hex_from_string(resp));
}

void test_websocket() {
    // start server
    sl::pion::http_server server(2, TCP_PORT);
//    server.add_websocket_handler("WSOPEN", "/mirror", wsopen);
    server.add_websocket_handler("WSMESSAGE", "/mirror", wsmsg);
//    server.add_websocket_handler("WSCLOSE", "/mirror", wsclose);
    server.start();

    // connect to server
    asio::io_service io_service;
    asio::ip::tcp::socket socket{io_service};
    asio::ip::tcp::endpoint endpoint{asio::ip::address_v4::from_string("127.0.0.1"), TCP_PORT};
    socket.connect(endpoint);

    // ws handshake
    auto buf = std::string();
    buf.resize(1024);
    auto handshake = make_handshake();
    socket.send(asio::buffer(handshake));
    socket.receive(asio::buffer(std::addressof(buf.front()), buf.length()));

    // checks
//    for (size_t i = 0; i < 3; i++) {
    check_hello(socket);
    check_tcp_fragment_1_byte(socket);
    check_tcp_fragment_split(socket);
    check_continuation(socket);
    check_broadcast(server, socket);
    check_long(socket);
//    }
    check_close(socket);

    socket.close();
    server.stop();
}

int main() {
    try {
        test_websocket();
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}
