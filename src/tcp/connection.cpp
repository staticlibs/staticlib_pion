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

#include "pion/tcp/connection.hpp"

#include "asio.hpp"
#ifdef PION_HAVE_SSL
#ifdef PION_XCODE
// ignore openssl warnings if building with XCode
#pragma GCC system_header
#endif
#include "asio/ssl.hpp"
#endif


namespace pion {
namespace tcp {

std::shared_ptr<connection> connection::create(asio::io_service& io_service,
        ssl_context_type& ssl_context,
        const bool ssl_flag,
        connection_handler finished_handler) {
    return std::shared_ptr<connection>(
            new connection(io_service, ssl_context, ssl_flag, finished_handler));
}

connection::connection(asio::io_service& io_service, ssl_context_type& ssl_context,
        const bool ssl_flag, connection_handler finished_handler) :
#ifdef PION_HAVE_SSL
m_ssl_socket(io_service, ssl_context), 
m_ssl_flag(ssl_flag),
#else
m_ssl_socket(io_service), 
m_ssl_flag(false),
#endif
m_lifecycle(LIFECYCLE_CLOSE),
m_finished_handler(finished_handler) {
#ifndef PION_HAVE_SSL
    (void) ssl_context;
    (void) ssl_flag;
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

std::size_t connection::read_some(asio::error_code& ec) {
#ifdef PION_HAVE_SSL
    if (get_ssl_flag())
        return m_ssl_socket.read_some(asio::buffer(m_read_buffer), ec);
    else
#endif      
        return m_ssl_socket.next_layer().read_some(asio::buffer(m_read_buffer), ec);
}

void connection::finish() {
    connection_ptr conn = shared_from_this();
    if (m_finished_handler) m_finished_handler(conn);
}

bool connection::get_ssl_flag() const {
    return m_ssl_flag;
}

void connection::set_lifecycle(lifecycle_type t) {
    m_lifecycle = t;
}

connection::lifecycle_type connection::get_lifecycle() const {
    return m_lifecycle;
}

bool connection::get_keep_alive() const {
    return m_lifecycle != LIFECYCLE_CLOSE;
}

bool connection::get_pipelined() const {
    return m_lifecycle == LIFECYCLE_PIPELINED;
}

connection::read_buffer_type& connection::get_read_buffer() {
    return m_read_buffer;
}

void connection::save_read_pos(const char *read_ptr, const char *read_end_ptr) {
    m_read_position.first = read_ptr;
    m_read_position.second = read_end_ptr;
}

void connection::load_read_pos(const char *&read_ptr, const char *&read_end_ptr) const {
    read_ptr = m_read_position.first;
    read_end_ptr = m_read_position.second;
}

asio::ip::tcp::endpoint connection::get_remote_endpoint() const {
    asio::ip::tcp::endpoint remote_endpoint;
    try {
        // const_cast is required since lowest_layer() is only defined non-const in asio
        remote_endpoint = const_cast<ssl_socket_type&> (m_ssl_socket).lowest_layer().remote_endpoint();
    } catch (asio::system_error& /* e */) {
        // do nothing
    }
    return remote_endpoint;
}

asio::ip::address connection::get_remote_ip() const {
    return get_remote_endpoint().address();
}

unsigned short connection::get_remote_port() const {
    return get_remote_endpoint().port();
}

asio::io_service& connection::get_io_service() {
    return m_ssl_socket.lowest_layer().get_io_service();
}

connection::socket_type& connection::get_socket() {
    return m_ssl_socket.next_layer();
}

connection::ssl_socket_type& connection::get_ssl_socket() {
    return m_ssl_socket;
}

const connection::socket_type& connection::get_socket() const {
    return const_cast<ssl_socket_type&> (m_ssl_socket).next_layer();
}

const connection::ssl_socket_type& connection::get_ssl_socket() const {
    return m_ssl_socket;
}

#ifndef PION_HAVE_SSL

connection::ssl_socket_type::ssl_socket_type(asio::io_service& io_service) : 
m_socket(io_service) { }

connection::socket_type& connection::ssl_socket_type::next_layer() {
    return m_socket;
}

const connection::socket_type& connection::ssl_socket_type::next_layer() const {
    return m_socket;
}

connection::socket_type::lowest_layer_type& connection::ssl_socket_type::lowest_layer() {
    return m_socket.lowest_layer();
}

const connection::socket_type::lowest_layer_type& connection::ssl_socket_type::lowest_layer() const {
    return m_socket.lowest_layer();
}

void connection::ssl_socket_type::shutdown() { }

#endif // PION_HAVE_SSL


} // end namespace tcp
} // end namespace pion
