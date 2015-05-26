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

#include <string>
#include <memory>
#include <array>
#include <functional>

#include "asio.hpp"
#ifdef PION_HAVE_SSL
#ifdef PION_XCODE
// ignore openssl warnings if building with XCode
#pragma GCC system_header
#endif
#include "asio/ssl.hpp"
#endif

#include "pion/noncopyable.hpp"
#include "pion/config.hpp"
#include "pion/algorithm.hpp"
#include "pion/tcp/connection.hpp"

namespace pion {
namespace tcp {

std::shared_ptr<connection> connection::create(asio::io_service& io_service,
        ssl_context_type& ssl_context,
        const bool ssl_flag,
        connection_handler finished_handler) {
    return std::shared_ptr<connection>(
            new connection(io_service, ssl_context, ssl_flag, finished_handler));
}

connection::connection(asio::io_service& io_service, const bool ssl_flag) :
#ifdef PION_HAVE_SSL
m_ssl_context(asio::ssl::context::sslv23),
m_ssl_socket(io_service, m_ssl_context),
m_ssl_flag(ssl_flag),
#else
m_ssl_context(0),
m_ssl_socket(io_service),
m_ssl_flag(false),
#endif
m_lifecycle(LIFECYCLE_CLOSE) {
#ifndef PION_HAVE_SSL
    (void) ssl_flag;
    (void) m_ssl_context;
#endif            
    save_read_pos(NULL, NULL);
}

connection::connection(asio::io_service& io_service, ssl_context_type& ssl_context) :
#ifdef PION_HAVE_SSL
m_ssl_context(asio::ssl::context::sslv23),
m_ssl_socket(io_service, ssl_context), m_ssl_flag(true),
#else
m_ssl_context(0),
m_ssl_socket(io_service),
m_ssl_flag(false),
#endif
m_lifecycle(LIFECYCLE_CLOSE) {
#ifndef PION_HAVE_SSL
    (void) ssl_context;
#endif            
    save_read_pos(NULL, NULL);
}

connection::~connection() {
    close();
}

bool connection::is_open() const {
    return const_cast<ssl_socket_type&> (m_ssl_socket).lowest_layer().is_open();
}

void connection::close() {
    if (is_open()) {
        try {
            // shutting down SSL will wait forever for a response from the remote end,
            // which causes it to hang indefinitely if the other end died unexpectedly
            // if (get_ssl_flag()) m_ssl_socket.shutdown();

            // windows seems to require this otherwise it doesn't
            // recognize that connections have been closed
            m_ssl_socket.next_layer().shutdown(asio::ip::tcp::socket::shutdown_both);
        } catch (...) {
        } // ignore exceptions

        // close the underlying socket (ignore errors)
        asio::error_code ec;
        m_ssl_socket.next_layer().close(ec);
    }
}

// there is no good way to do this on windows until vista or later (0x0600)
// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/basic_stream_socket/cancel/overload2.html
// note that the asio docs are misleading because close() is not thread-safe,
// and the suggested #define statements cause WAY too much trouble and heartache
void connection::cancel() {
#if !defined(_MSC_VER) || (_WIN32_WINNT >= 0x0600)
    asio::error_code ec;
    m_ssl_socket.next_layer().cancel(ec);
#endif
}

} // end namespace tcp
} // end namespace pion
