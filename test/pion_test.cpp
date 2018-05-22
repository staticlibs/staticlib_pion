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

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_response_writer.hpp"
#include "staticlib/pion/http_server.hpp"
#include "staticlib/pion/http_filter_chain.hpp"

#ifdef STATICLIB_PION_USE_LOG4CPLUS
#include <log4cplus/logger.h>
#include <log4cplus/consoleappender.h>
#endif // STATICLIB_PION_USE_LOG4CPLUS


const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8080;

#ifdef STATICLIB_PION_USE_LOG4CPLUS
const std::string CONSOLE_APPENDER_LAYOUT = "%d{%H:%M:%S} [%-5p %-15.15c] %m%n";

log4cplus::SharedAppenderPtr create_console_appender() {
    log4cplus::SharedAppenderPtr res{new log4cplus::ConsoleAppender()};
    res->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(CONSOLE_APPENDER_LAYOUT)));
    return res;
}
#endif // STATICLIB_PION_USE_LOG4CPLUS

void hello_service(sl::pion::http_request_ptr& req, sl::pion::tcp_connection_ptr& conn) {
    auto writer = sl::pion::http_response_writer::create(conn, req);
    writer << "Hello World!\n";
    writer->send();
}

void hello_service_post(sl::pion::http_request_ptr& req, sl::pion::tcp_connection_ptr& conn) {
    auto writer = sl::pion::http_response_writer::create(conn, req);
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
    sl::pion::http_response_writer_ptr writer;
    std::ifstream stream;
    std::array<char, 8192> buf;
    std::mutex mutex;
    
public:
    FileSender(const std::string& filename, sl::pion::http_response_writer_ptr writer) : 
    writer(writer),
    stream(filename, std::ios::in | std::ios::binary) {
        stream.exceptions(std::ifstream::badbit);
    }
    
    void send() {
        std::error_code ec{};
        handle_write(ec, 0);
    }
    
    void handle_write(const std::error_code& ec, std::size_t /* bytes_written */) {
        std::lock_guard<std::mutex> lock{mutex};
        if (!ec) {
            stream.read(buf.data(), buf.size());
            writer->clear();
            writer->write_no_copy(buf.data(), static_cast<size_t>(stream.gcount()));
            if (stream) {
                auto self = shared_from_this();
                writer->send_chunk([self](const std::error_code& ec, size_t bt) {
                    self->handle_write(ec, bt);
                });
            } else {
                writer->send_final_chunk();
            }
        } else {
            // make sure it will get closed
            writer->get_connection()->set_lifecycle(sl::pion::tcp_connection::LIFECYCLE_CLOSE);
        }
    }
};

void file_upload_resource(sl::pion::http_request_ptr& req, sl::pion::tcp_connection_ptr& conn) {
    auto ph = req->get_payload_handler<FileWriter>();
    if (ph) {
        ph->close();
    } else {
        std::cout << "No payload handler found in main handler" << std::endl;
    }
    auto writer = sl::pion::http_response_writer::create(conn, req);
    auto fs = std::make_shared<FileSender>("uploaded.dat", writer);
    fs->send();
}

FileWriter file_upload_payload_handler_creator(sl::pion::http_request_ptr& req) {
    (void) req;
    return FileWriter{"uploaded.dat"};
}

void logging_filter1(sl::pion::http_request_ptr& request, sl::pion::tcp_connection_ptr& conn, 
        sl::pion::http_filter_chain& chain) {
    std::cout << "Hi from filter 1 for [" << request->get_resource() << "]" << std::endl;
    chain.do_filter(request, conn);
}

void logging_filter2(sl::pion::http_request_ptr& request, sl::pion::tcp_connection_ptr& conn,
        sl::pion::http_filter_chain& chain) {
    std::cout << "Hi from filter 2 for [" << request->get_resource() << "]" << std::endl;
    chain.do_filter(request, conn);
}

void test_pion() {
#ifdef STATICLIB_PION_USE_LOG4CPLUS
    #ifdef STATICLIB_PION_USE_LOG4CPLUS_STATIC
    // need initialization with static log4cplus
    log4cplus::initialize();
    #endif // STATICLIB_PION_USE_LOG4CPLUS_STATIC
    auto fa = create_console_appender();
    log4cplus::Logger::getRoot().addAppender(fa);
    log4cplus::Logger::getRoot().setLogLevel(log4cplus::ALL_LOG_LEVEL);
    log4cplus::Logger::getInstance("pion").setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else // std out logging
    STATICLIB_PION_LOG_SETLEVEL_INFO(STATICLIB_PION_GET_LOGGER("staticlib.pion"))
#endif // STATICLIB_PION_USE_LOG4CPLUS    
    // pion
    sl::pion::http_server server(2, TCP_PORT);
    server.add_handler("GET", "/hello", hello_service);
    server.add_handler("POST", "/hello/post", hello_service_post);
    server.add_filter("POST", "/hello", logging_filter1);
    server.add_filter("POST", "/", logging_filter2);
    server.add_handler("POST", "/fu", file_upload_resource);
    server.add_payload_handler("POST", "/fu", file_upload_payload_handler_creator);
    server.add_handler("POST", "/fu1", file_upload_resource);
    server.get_scheduler().set_thread_stop_hook([]() STATICLIB_NOEXCEPT {
        std::cout << "Thread stopped. " << std::endl;
    });
    server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    server.stop(true);
}

int main() {    
    try {
        test_pion();
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}
