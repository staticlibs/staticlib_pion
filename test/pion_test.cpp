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


class FileUploadResource {
    std::ofstream stream;
    std::mutex write_mutex;
    
public:  
    
    FileUploadResource() : stream("uploaded.dat", std::ios::out | std::ios::binary) { 
        stream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }
    
    FileUploadResource& operator()(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
        stream.close();
        auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
        auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
        writer << "UPLOADED\n";
        writer->send();

        return *this;
    }
    
    FileUploadResource& operator()(const char* s, std::size_t n) {
//        throw std::runtime_error("AAAA");
        std::unique_lock<std::mutex> lock{write_mutex, std::try_to_lock};
        stream.write(s, n);
        return *this;
    }
};

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
    FileUploadResource fr{};
    web_server.add_resource("/fu", std::ref(fr));
    web_server.add_payload_handler("/fu", std::ref(fr));
    web_server.add_resource("/fu1", std::ref(fr));
    web_server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    
    return 0;
}

