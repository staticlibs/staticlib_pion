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

#include "staticlib/support.hpp"

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_response_writer.hpp"
#include "staticlib/pion/http_server.hpp"

const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8080;

void hello_service(sl::pion::http_request_ptr, sl::pion::response_writer_ptr resp) {
    resp->write("Hello World!\n");
    resp->send(std::move(resp));
}

void hello_service_post(sl::pion::http_request_ptr, sl::pion::response_writer_ptr resp) {
    resp->write("Hello POST!\n");
    resp->send(std::move(resp));
}

class file_writer {
    mutable std::unique_ptr<std::ofstream> stream;
 
public:
    // copy constructor is required due to std::function limitations
    // but it doesn't used by server implementation
    // move-only payload handlers can use move logic
    // instead of copy one (with mutable member fields)
    // in MSVC only moved-to instance will be used
    // in GCC copy constructor won't be called at all
    file_writer(const file_writer& other) :
    stream(std::move(other.stream)) { }

    file_writer& operator=(const file_writer&) = delete;

    file_writer(file_writer&& other) :
    stream(std::move(other.stream)) { }

    file_writer& operator=(file_writer&&) = delete;

    file_writer(const std::string& filename) {
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

class file_sender {
    sl::pion::response_writer_ptr writer;
    std::ifstream stream;
    std::array<char, 8192> buf;

public:
    file_sender(const std::string& filename, sl::pion::response_writer_ptr writer) : 
    writer(std::move(writer)),
    stream(filename, std::ios::in | std::ios::binary) {
        stream.exceptions(std::ifstream::badbit);
    }

    static void send(std::unique_ptr<file_sender> self) {
        std::error_code ec;
        handle_write(std::move(self), ec, 0);
    }

    static void handle_write(std::unique_ptr<file_sender> self,
            const std::error_code& ec, std::size_t /* bytes_written */) {
        if (!ec) {
            self->stream.read(self->buf.data(), self->buf.size());
            self->writer->clear();
            self->writer->write_nocopy({self->buf.data(), self->stream.gcount()});
            if (self->stream) {
                auto& wr = self->writer;
                auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
                wr->send_chunk(
                    [self_shared](const std::error_code& ec, size_t bt) {
                        auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                        if (self.get()) {
                            handle_write(std::move(self), ec, bt);
                        }
                    });
            } else {
                self->writer->send_final_chunk(std::move(self->writer));
            }
        } else {
            // make sure it will get closed
            self->writer->get_connection()->set_lifecycle(sl::pion::tcp_connection::LIFECYCLE_CLOSE);
        }
    }
};

void file_upload_resource(sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
    auto ph = req->get_payload_handler<file_writer>();
    if (ph) {
        ph->close();
    } else {
        std::cout << "No payload handler found in main handler" << std::endl;
    }
    auto fs = sl::support::make_unique<file_sender>("uploaded.dat", std::move(resp));
    fs->send(std::move(fs));
}

file_writer file_upload_payload_handler_creator(sl::pion::http_request_ptr& req) {
    (void) req;
    return file_writer{"uploaded.dat"};
}

void test_pion() {
    // pion
    sl::pion::http_server server(2, TCP_PORT);
    server.add_handler("GET", "/hello", hello_service);
    server.add_handler("POST", "/hello/post", hello_service_post);
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
