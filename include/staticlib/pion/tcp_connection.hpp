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

// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef STATICLIB_PION_TCP_CONNECTION_HPP
#define STATICLIB_PION_TCP_CONNECTION_HPP

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "asio.hpp"
#include "asio/ssl.hpp"

#include "staticlib/config.hpp"

#include "staticlib/pion/algorithm.hpp"

namespace staticlib { 
namespace pion {

/**
 * Represents a single tcp connection
 */
class tcp_connection : public std::enable_shared_from_this<tcp_connection> {

public:

    /**
     * Data type for the connection's current_lifecycle state
     */
    enum class lifecycle {
        close, 
        keepalive, 
        pipelined
    };

    /**
     * Data type for a function that handles TCP connection objects
     */
    using connection_handler = std::function<void(std::shared_ptr<tcp_connection>&)>;

    /**
     * Data type for an I/O read buffer
     */
    using read_buffer_type = std::array<char, 8192>;

    /**
     * Data type for a socket connection
     */
    using socket_type = asio::ip::tcp::socket;

    /**
     * Data type for an SSL socket connection
     */
    using ssl_socket_type = asio::ssl::stream<asio::ip::tcp::socket>;

    /**
     * Data type for SSL configuration context
     */
    using ssl_context_type = asio::ssl::context;

private:

    /**
     * SSL connection socket
     */
    ssl_socket_type ssl_socket;

    /**
     * True if the connection is encrypted using SSL
     */
    bool ssl_flag;

    /**
     * Buffer used for reading data from the TCP connection
     */
    read_buffer_type read_buffer;

    /**
     * Saved read position bookmark
     */
    std::pair<const char*, const char*> read_position;

    /**
     * Lifecycle state for the connection
     */
    lifecycle current_lifecycle;

    /**
     * Function called when a server has finished handling the connection
     */
    connection_handler finished_handler;

    /**
     * Strand used to synchronize all async operations over this connection
     */
    asio::io_service::strand strand;

    /**
     * Timer that can be used with IO operations over this connection
     */
    asio::steady_timer timer;

public:

    /**
     * Constructor to be used with `std::make_shared`
     *
     * @param io_service asio service associated with the connection
     * @param ssl_context asio ssl context associated with the connection
     * @param ssl_flag if true then the connection will be encrypted using SSL 
     * @param finished_handler function called when a server has finished
     *                         handling the connection
     */
    tcp_connection(asio::io_service& io_service, ssl_context_type& ssl_context, const bool ssl_flag_in,
            connection_handler finished_handler_in) :
    ssl_socket(io_service, ssl_context), 
    ssl_flag(ssl_flag_in),
    current_lifecycle(lifecycle::close),
    finished_handler(finished_handler_in),
    strand(io_service),
    timer(io_service) {
        save_read_pos(nullptr, nullptr);
    }

    /**
     * Deleted copy constructor
     */
    tcp_connection(const tcp_connection&) = delete;

    /**
     * Deleted copy assignment operator
     */
    tcp_connection& operator=(const tcp_connection&) = delete;

    /**
     * Virtual destructor
     */
    virtual ~tcp_connection() {
        close();
    }

    /**
     * Returns true if the connection is currently open
     * 
     * @return true if the connection is currently open
     */
    bool is_open() const {
        return const_cast<ssl_socket_type&> (ssl_socket).lowest_layer().is_open();
    }

    /**
     * Closes the tcp socket and cancels any pending asynchronous operations
     */
    void close() {
        if (is_open()) {
            try {
                // shutting down SSL will wait forever for a response from the remote end,
                // which causes it to hang indefinitely if the other end died unexpectedly
                // if (get_ssl_flag()) ssl_socket.shutdown();

                // windows seems to require this otherwise it doesn't
                // recognize that connections have been closed
                ssl_socket.next_layer().shutdown(asio::ip::tcp::socket::shutdown_both);
            } catch (...) {
            } // ignore exceptions

            // close the underlying socket (ignore errors)
            std::error_code ec;
            ssl_socket.next_layer().close(ec);
        }
    }

    /**
     * Cancels any asynchronous operations pending on the socket.
     */
    void cancel() {
        // there is no good way to do this on windows until vista or later (0x0600)
        // http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/basic_stream_socket/cancel/overload2.html
        // note that the asio docs are misleading because close() is not thread-safe,
        // and the suggested #define statements cause WAY too much trouble and heartache
        #if !defined(_MSC_VER) || (_WIN32_WINNT >= 0x0600)
            std::error_code ec;
            ssl_socket.next_layer().cancel(ec);
        #endif // !WINXP
    }

    /**
     * Cancels timer
     */
    void cancel_timer() {
        #if !defined(_MSC_VER) || (_WIN32_WINNT >= 0x0600)
            std::error_code ec;
            timer.cancel(ec);
        #endif // !WINXP
    }

    /**
     * Asynchronously accepts a new tcp connection
     *
     * @param tcp_acceptor object used to accept new connections
     * @param handler called after a new connection has been accepted
     *
     * @see asio::basic_socket_acceptor::async_accept()
     */
    template <typename AcceptHandler>
    void async_accept(asio::ip::tcp::acceptor& tcp_acceptor, AcceptHandler handler) {
        tcp_acceptor.async_accept(ssl_socket.lowest_layer(), handler);
    }

    /**
     * Asynchronously performs server-side SSL handshake for a new connection
     *
     * @param handler called after the ssl handshake has completed
     *
     * @see asio::ssl::stream::async_handshake()
     */
    template <typename SSLHandshakeHandler>
    void async_handshake_server(SSLHandshakeHandler handler) {
        ssl_socket.async_handshake(asio::ssl::stream_base::server, handler);
        ssl_flag = true;
    }

    /**
     * Asynchronously reads some data into the connection's read buffer 
     *
     * @param handler called after the read operation has completed
     *
     * @see asio::basic_stream_socket::async_read_some()
     */
    template <typename ReadHandler>
    void async_read_some(ReadHandler handler) {
        if (get_ssl_flag()) {
            ssl_socket.async_read_some(asio::buffer(read_buffer), handler);
        } else {
            ssl_socket.next_layer().async_read_some(asio::buffer(read_buffer), handler);
        }
    }

    /**
     * Asynchronously writes data to the connection
     *
     * @param buffers one or more buffers containing the data to be written
     * @param handler called after the data has been written
     *
     * @see asio::async_write()
     */
    template <typename ConstBufferSequence, typename write_handler_t>
    void async_write(const ConstBufferSequence& buffers, write_handler_t handler) {
        if (get_ssl_flag()) {
            asio::async_write(ssl_socket, buffers, handler);
        } else {
            asio::async_write(ssl_socket.next_layer(), buffers, handler);
        }
    }

    /**
     * This function should be called when a server has finished handling the connection
     */
    void finish() {
        auto conn = shared_from_this();
        if (finished_handler) {
            finished_handler(conn);
        }
    }

    /**
     * Returns true if the connection is encrypted using SSL
     * 
     * @return true if the connection is encrypted using SSL
     */
    bool get_ssl_flag() const {
        return ssl_flag;
    }

    /**
     * Sets the lifecycle for the connection
     * 
     * @param lcycle lifecycle type name
     */
    void set_lifecycle(lifecycle lcycle) {
        current_lifecycle = lcycle;
    }

    /**
     * Returns true if the connection should be kept alive
     * 
     * @return 
     */
    bool get_keep_alive() const {
        return current_lifecycle != lifecycle::close;
    }

    /**
     * Returns true if the HTTP requests are pipelined
     * 
     * @return true if the HTTP requests are pipelined
     */
    bool is_pipelined() const {
        return current_lifecycle == lifecycle::pipelined;
    }

    /**
     * Returns the buffer used for reading data from the TCP connection
     * 
     * @return buffer used for reading data from the TCP connection
     */
    read_buffer_type& get_read_buffer() {
        return read_buffer;
    }

    /**
     * Saves a read position bookmark
     *
     * @param read_ptr points to the next character to be consumed in the read_buffer
     * @param read_end_ptr points to the end of the read_buffer (last byte + 1)
     */
    void save_read_pos(const char *read_ptr, const char *read_end_ptr) {
        read_position.first = read_ptr;
        read_position.second = read_end_ptr;
    }

    /**
     * Loads a read position bookmark
     *
     * @param read_ptr points to the next character to be consumed in the read_buffer
     * @param read_end_ptr points to the end of the read_buffer (last byte + 1)
     */
    void load_read_pos(const char *&read_ptr, const char *&read_end_ptr) const {
        read_ptr = read_position.first;
        read_end_ptr = read_position.second;
    }

    /**
     * Returns an ASIO endpoint for the client connection
     * 
     * @return endpoint
     */
    asio::ip::tcp::endpoint get_remote_endpoint() const {
        asio::ip::tcp::endpoint remote_endpoint;
        try {
            // const_cast is required since lowest_layer() is only defined non-const in asio
            remote_endpoint = const_cast<ssl_socket_type&> (ssl_socket).lowest_layer().remote_endpoint();
        } catch (asio::system_error& /* e */) {
            // do nothing
        }
        return remote_endpoint;
    }

    /**
     * Returns the client's IP address
     * 
     * @return client's IP address
     */
    asio::ip::address get_remote_ip() const {
        return get_remote_endpoint().address();
    }

    /**
     * Returns the client's port number
     * 
     * @return client's port number
     */
    unsigned short get_remote_port() const {
        return get_remote_endpoint().port();
    }

    /**
     * Returns reference to the io_service used for async operations
     * 
     * @return io_service used for async operations
     */
    asio::io_service& get_io_service() {
#if ASIO_VERSION >= 101400
        return static_cast<asio::io_service&>(ssl_socket.lowest_layer().get_executor().context());
#else
        return ssl_socket.lowest_layer().get_io_service();
#endif
    }

    /**
     * Returns non-const reference to underlying TCP socket object
     * 
     * @return underlying TCP socket object
     */
    socket_type& get_socket() {
        return ssl_socket.next_layer();
    }

    /**
     * Returns non-const reference to underlying SSL socket object
     * 
     * @return underlying SSL socket object
     */
    ssl_socket_type& get_ssl_socket() {
        return ssl_socket;
    }

    /**
     * Returns the strand that can be used with this connection
     * 
     * @return the strand that can be used with this connection
     */
    asio::io_service::strand& get_strand() {
        return strand;
    }

    /**
     * Returns the timer that can be used with this connection
     * 
     * @return the timer that can be used with this connection
     */
    asio::steady_timer& get_timer() {
        return timer;
    }

};

/**
 * Data type for a connection pointer
 */
using tcp_connection_ptr = std::shared_ptr<tcp_connection>;

} // namespace
}

#endif // STATICLIB_PION_TCP_CONNECTION_HPP
