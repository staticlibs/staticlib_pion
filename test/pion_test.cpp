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
#include "staticlib/log4cplus_utils.hpp"
#endif // PION_USE_LOG4CPLUS

namespace { // anonymous

const uint16_t SECONDS_TO_RUN = 10000;
const uint16_t TCP_PORT = 8080;

void helloService(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    writer << "Hello World!\n";
    writer->send();
}

class FileWriter {
    std::shared_ptr<std::ofstream> stream;
    
public:
    
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

void file_upload_resource(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto ph = http_request_ptr->get_payload_handler<FileWriter>();
    if (ph) {
        ph->close();
    } else {
        std::cout << "No payload handler found in main handler" << std::endl;
    }
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    writer << "UPLOADED\n";
    writer->send();
}

void file_upload_payload_handler_creator(pion::http::request_ptr& http_request_ptr) {
    http_request_ptr->set_payload_handler(FileWriter{"uploaded.dat"});
}

} // namespace

int main() {
#ifdef PION_USE_LOG4CPLUS
    log4cplus::initialize();
    auto fa = staticlib::log::create_console_appender();
    log4cplus::Logger::getRoot().addAppender(fa);
    log4cplus::Logger::getRoot().setLogLevel(log4cplus::ALL_LOG_LEVEL);
    log4cplus::Logger::getInstance("pion").setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else // std out logging
    PION_LOG_SETLEVEL_INFO(PION_GET_LOGGER("pion"))
#endif // PION_USE_LOG4CPLUS    
    // pion
    pion::http::streaming_server web_server(2, TCP_PORT);
    web_server.add_resource("/", helloService);
    web_server.add_resource("/fu", file_upload_resource);
    web_server.add_payload_handler("/fu", file_upload_payload_handler_creator);
    web_server.add_resource("/fu1", file_upload_resource);
    web_server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    
    return 0;
}
