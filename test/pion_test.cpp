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
 * File:   pion_test.cpp
 * Author: alex
 *
 * Created on March 3, 2015, 9:22 AM
 */

#include <iostream>
#include <memory>
#include <utility>
#include <functional>
#include <thread>
#include <fstream>
#include <chrono>
#include <cstdint>

#include "asio.hpp"

#include <pion/config.hpp>
#include <pion/logger.hpp>
#include <pion/http/response_writer.hpp>
#include <pion/http/streaming_server.hpp>

#ifdef PION_USE_LOG4CPLUS
#include <log4cplus/logger.h>
#include <log4cplus/consoleappender.h>
#endif // PION_USE_LOG4CPLUS

namespace { // anonymous

const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8080;

#ifdef PION_USE_LOG4CPLUS
const std::string CONSOLE_APPENDER_LAYOUT = "%d{%H:%M:%S} [%-5p %-15.15c] %m%n";

log4cplus::SharedAppenderPtr create_console_appender() {
    log4cplus::SharedAppenderPtr res{new log4cplus::ConsoleAppender()};
    res->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(CONSOLE_APPENDER_LAYOUT)));
    return res;
}
#endif // PION_USE_LOG4CPLUS

void hello_service(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    writer << "Hello World!\n";
    writer->send();
}

void hello_service_post(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    writer << "Hello POST!\n";
    writer->send();
}

class FileWriter {
    mutable std::unique_ptr<std::ofstream> stream;
    
public:
    // copy constructor is required due to std::function limitations
    // but it doesn't used by server implementation
    // move-only payload handlers can use move logic
    // instead of copy one (with mutable member fields)
    // in MSVC only moved-to instance will be used
    // in GCC copy constructor won't be called at all
    FileWriter(const FileWriter& other) :
    stream(std::move(other.stream)) { }
    
    FileWriter& operator=(const FileWriter&) = delete;  

    FileWriter(FileWriter&& other) :
    stream(std::move(other.stream)) { }
            
    FileWriter& operator=(FileWriter&&) = delete;
    
    FileWriter(const std::string& filename) {
        stream = std::unique_ptr<std::ofstream>{new std::ofstream{filename, std::ios::out | std::ios::binary}};
        stream->exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }
    
    void operator()(const char* s, std::size_t n) {
        stream->write(s, n);
    }
    
    void close() {
        std::cout << "I am closed" << std::endl;
        stream->close();
    }
};

class FileSender : public std::enable_shared_from_this<FileSender> {
    pion::http::writer_ptr writer;
    std::ifstream stream;
    std::array<char, 8192> buf;
    std::mutex mutex;
    
public:
    FileSender(const std::string& filename, pion::http::writer_ptr writer) : 
    writer(writer),
    stream(filename, std::ios::in | std::ios::binary) {
        stream.exceptions(std::ifstream::badbit);
    }
    
    void send() {
        asio::error_code ec{};
        handle_write(ec, 0);
    }
    
    void handle_write(const asio::error_code& ec, std::size_t /* bytes_written */) {
        std::lock_guard<std::mutex> lock{mutex};
        if (!ec) {
            stream.read(buf.data(), buf.size());
            writer->clear();
            writer->write_no_copy(buf.data(), static_cast<size_t>(stream.gcount()));
            if (stream) {
                writer->send_chunk(std::bind(&FileSender::handle_write, shared_from_this(), 
                        std::placeholders::_1, std::placeholders::_2));
            } else {
                writer->send_final_chunk();
            }
        } else {
            // make sure it will get closed
            writer->get_connection()->set_lifecycle(pion::tcp::connection::LIFECYCLE_CLOSE);
            // log error
        }
    }
    
};

void file_upload_resource(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto ph = http_request_ptr->get_payload_handler<FileWriter>();
    if (ph) {
        ph->close();
    } else {
        std::cout << "No payload handler found in main handler" << std::endl;
    }
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    auto fs = std::make_shared<FileSender>("uploaded.dat", writer);
    fs->send();
}

FileWriter file_upload_payload_handler_creator(pion::http::request_ptr& http_request_ptr) {
    (void) http_request_ptr;
    return FileWriter{"uploaded.dat"};
}

} // namespace

int main() {
#ifdef PION_USE_LOG4CPLUS
    #ifdef PION_USE_LOG4CPLUS_STATIC
    // need initialization with static log4cplus
    log4cplus::initialize();
    #endif // PION_USE_LOG4CPLUS_STATIC
    auto fa = create_console_appender();
    log4cplus::Logger::getRoot().addAppender(fa);
    log4cplus::Logger::getRoot().setLogLevel(log4cplus::ALL_LOG_LEVEL);
    log4cplus::Logger::getInstance("pion").setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else // std out logging
    PION_LOG_SETLEVEL_INFO(PION_GET_LOGGER("pion"))
#endif // PION_USE_LOG4CPLUS    
    // pion
    pion::http::streaming_server web_server(2, TCP_PORT);
    web_server.add_method_specific_resource("GET", "/hello", hello_service);
    web_server.add_method_specific_resource("POST", "/hello", hello_service_post);
    web_server.add_method_specific_resource("POST", "/fu", file_upload_resource);
    web_server.add_method_specific_payload_handler("POST", "/fu", file_upload_payload_handler_creator);
    web_server.add_method_specific_resource("POST", "/fu1", file_upload_resource);
    web_server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    web_server.stop(true);
    
    return 0;
}
