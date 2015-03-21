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
#include <chrono>
#include <cstdint>

#include "asio.hpp"

#include <pion/http/response_writer.hpp>
#include <pion/http/server.hpp>

//#include <log4cplus/logger.h>

namespace { // anonymous

const uint16_t SECONDS_TO_RUN = 10;
const uint16_t TCP_PORT = 8080;

void helloService(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    auto msg = std::string{"<html><body>Hello World!</body></html>\n"};
    writer->write_no_copy(msg);
    writer->send();
}

} // namespace

//#ifdef STATICLIB_WINDOWS
//namespace boost {
//// http://lists.boost.org/Archives/boost/2007/11/130440.php
//    void tss_cleanup_implemented() {
//        // todo: investigate me
//    }
//}
//#endif // STATICLIB_WINDOWS

int main() {
    // logging
//    log4cplus::initialize();
//    auto fa = staticlib::log::create_console_appender();
//    log4cplus::Logger::getRoot().addAppender(fa);
//    log4cplus::Logger::getRoot().setLogLevel(log4cplus::ALL_LOG_LEVEL);
//    log4cplus::Logger::getInstance("pion").setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    // pion
    asio::ip::tcp::endpoint cfg_endpoint(asio::ip::tcp::v4(), TCP_PORT);
    pion::http::server web_server(cfg_endpoint);
    web_server.add_resource("/", helloService);
    web_server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    
    return 0;
}

