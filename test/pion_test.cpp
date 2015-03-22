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

#include <pion/config.hpp>
#include <pion/logger.hpp>
#include <pion/http/response_writer.hpp>
#include <pion/http/server.hpp>

#ifdef PION_USE_LOG4CPLUS
#include <log4cplus/logger.h>
#include "staticlib/log4cplus_utils.hpp"
#endif // PION_USE_LOG4CPLUS

namespace { // anonymous

const uint16_t SECONDS_TO_RUN = 10;
const uint16_t TCP_PORT = 8080;

void helloService(pion::http::request_ptr& http_request_ptr, pion::tcp::connection_ptr& tcp_conn) {
    auto finfun = std::bind(&pion::tcp::connection::finish, tcp_conn);
    auto writer = pion::http::response_writer::create(tcp_conn, *http_request_ptr, finfun);
    writer << "Hello World!\n";
    writer->send();
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
    PION_LOG_SETLEVEL_DEBUG(PION_GET_LOGGER("pion"))
#endif // PION_USE_LOG4CPLUS    
    // pion
    asio::ip::tcp::endpoint cfg_endpoint(asio::ip::tcp::v4(), TCP_PORT);
    pion::http::server web_server(cfg_endpoint);
    web_server.add_resource("/", helloService);
    web_server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    
    return 0;
}

