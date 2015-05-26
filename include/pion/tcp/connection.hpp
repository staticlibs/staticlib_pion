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

#ifndef __PION_TCP_CONNECTION_HEADER__
#define __PION_TCP_CONNECTION_HEADER__

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

namespace pion {
namespace tcp {

/**
 * Represents a single tcp connection
 */
class connection : public std::enable_shared_from_this<connection>, private pion::noncopyable {    
    
public:

    /**
     * Data type for the connection's lifecycle state
     */
    enum lifecycle_type {
        LIFECYCLE_CLOSE, 
        LIFECYCLE_KEEPALIVE, 
        LIFECYCLE_PIPELINED
    };
    
    /**
     * Size of the read buffer
     */
    enum { READ_BUFFER_SIZE = 8192 };
    
    /**
     * Data type for a function that handles TCP connection objects
     */
    typedef std::function<void(std::shared_ptr<connection>)> connection_handler;
    
    /**
     * Data type for an I/O read buffer
     */
    typedef std::array<char, READ_BUFFER_SIZE> read_buffer_type;
    
    /**
     * Data type for a socket connection
     */
    typedef asio::ip::tcp::socket socket_type;

#ifdef PION_HAVE_SSL
    /**
     * Data type for an SSL socket connection
     */
    typedef asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_type;

    /**
     * Data type for SSL configuration context
     */
    typedef asio::ssl::context ssl_context_type;
#else
    class ssl_socket_type {
    public:
        ssl_socket_type(asio::io_service& io_service) : m_socket(io_service) {}
        inline socket_type& next_layer(void) { return m_socket; }
        inline const socket_type& next_layer(void) const { return m_socket; }
        inline socket_type::lowest_layer_type& lowest_layer(void) { return m_socket.lowest_layer(); }
        inline const socket_type::lowest_layer_type& lowest_layer(void) const { return m_socket.lowest_layer(); }
        inline void shutdown(void) {}
    private:
        socket_type  m_socket;
    };
    typedef int ssl_context_type;
#endif    
    
private:

    /**
     * Context object for the SSL connection socket
     */
    ssl_context_type m_ssl_context;

    /**
     * SSL connection socket
     */
    ssl_socket_type m_ssl_socket;

    /**
     * True if the connection is encrypted using SSL
     */
    bool m_ssl_flag;

    /**
     * Buffer used for reading data from the TCP connection
     */
    read_buffer_type m_read_buffer;

    /**
     * Saved read position bookmark
     */
    std::pair<const char*, const char*> m_read_position;

    /**
     * Lifecycle state for the connection
     */
    lifecycle_type m_lifecycle;

    /**
     * Function called when a server has finished handling the connection
     */
    connection_handler m_finished_handler;        

public:
    
    /**
     * creates new shared connection objects
     *
     * @param io_service asio service associated with the connection
     * @param ssl_context asio ssl context associated with the connection
     * @param ssl_flag if true then the connection will be encrypted using SSL 
     * @param finished_handler function called when a server has finished
     *                         handling the connection
     */
    static std::shared_ptr<connection> create(asio::io_service& io_service,
            ssl_context_type& ssl_context, const bool ssl_flag, connection_handler finished_handler);
    /**
     * creates a new connection object
     *
     * @param io_service asio service associated with the connection
     * @param ssl_flag if true then the connection will be encrypted using SSL 
     */
    explicit connection(asio::io_service& io_service, const bool ssl_flag = false);
    
    /**
     * creates a new connection object for SSL
     *
     * @param io_service asio service associated with the connection
     * @param ssl_context asio ssl context associated with the connection
     */
    connection(asio::io_service& io_service, ssl_context_type& ssl_context);

    /**
     * Virtual destructor
     */
    virtual ~connection();
    
    /**
     * Returns true if the connection is currently open
     * 
     * @return true if the connection is currently open
     */
    bool is_open() const;
    
    /**
     * Closes the tcp socket and cancels any pending asynchronous operations
     */
    void close();

    /**
     * Cancels any asynchronous operations pending on the socket.
     */
    void cancel();
        
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
        tcp_acceptor.async_accept(m_ssl_socket.lowest_layer(), handler);
    }

    /**
     * asynchronously performs server-side SSL handshake for a new connection
     *
     * @param handler called after the ssl handshake has completed
     *
     * @see asio::ssl::stream::async_handshake()
     */
    template <typename SSLHandshakeHandler>
    inline void async_handshake_server(SSLHandshakeHandler handler) {
#ifdef PION_HAVE_SSL
        m_ssl_socket.async_handshake(asio::ssl::stream_base::server, handler);
        m_ssl_flag = true;
#else
        (void) handler;
#endif
    }
    
    /**
     * asynchronously reads some data into the connection's read buffer 
     *
     * @param handler called after the read operation has completed
     *
     * @see asio::basic_stream_socket::async_read_some()
     */
    template <typename ReadHandler>
    inline void async_read_some(ReadHandler handler) {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            m_ssl_socket.async_read_some(asio::buffer(m_read_buffer),
                                         handler);
        else
#endif      
            m_ssl_socket.next_layer().async_read_some(asio::buffer(m_read_buffer),
                                         handler);
    }
    
    /**
     * asynchronously reads some data into the connection's read buffer 
     *
     * @param read_buffer the buffer to read data into
     * @param handler called after the read operation has completed
     *
     * @see asio::basic_stream_socket::async_read_some()
     */
    template <typename ReadBufferType, typename ReadHandler>
    inline void async_read_some(ReadBufferType read_buffer,
                                ReadHandler handler) {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            m_ssl_socket.async_read_some(read_buffer, handler);
        else
#endif      
            m_ssl_socket.next_layer().async_read_some(read_buffer, handler);
    }
    
    /**
     * reads some data into the connection's read buffer (blocks until finished)
     *
     * @param ec contains error code if the read fails
     * @return std::size_t number of bytes read
     *
     * @see asio::basic_stream_socket::read_some()
     */
    inline std::size_t read_some(asio::error_code& ec) {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            return m_ssl_socket.read_some(asio::buffer(m_read_buffer), ec);
        else
#endif      
            return m_ssl_socket.next_layer().read_some(asio::buffer(m_read_buffer), ec);
    }
    
    /**
     * reads some data into the connection's read buffer (blocks until finished)
     *
     * @param read_buffer the buffer to read data into
     * @param ec contains error code if the read fails
     * @return std::size_t number of bytes read
     *
     * @see asio::basic_stream_socket::read_some()
     */
    template <typename ReadBufferType>
    inline std::size_t read_some(ReadBufferType read_buffer,
                                 asio::error_code& ec)
    {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            return m_ssl_socket.read_some(read_buffer, ec);
        else
#endif      
            return m_ssl_socket.next_layer().read_some(read_buffer, ec);
    }
    
    /**
     * asynchronously reads data into the connection's read buffer until
     * completion_condition is met
     *
     * @param completion_condition determines if the read operation is complete
     * @param handler called after the read operation has completed
     *
     * @see asio::async_read()
     */
    template <typename CompletionCondition, typename ReadHandler>
    inline void async_read(CompletionCondition completion_condition,
                           ReadHandler handler)
    {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            asio::async_read(m_ssl_socket, asio::buffer(m_read_buffer),
                                    completion_condition, handler);
        else
#endif      
            asio::async_read(m_ssl_socket.next_layer(), asio::buffer(m_read_buffer),
                                    completion_condition, handler);
    }
            
    /**
     * asynchronously reads data from the connection until completion_condition
     * is met
     *
     * @param buffers one or more buffers into which the data will be read
     * @param completion_condition determines if the read operation is complete
     * @param handler called after the read operation has completed
     *
     * @see asio::async_read()
     */
    template <typename MutableBufferSequence, typename CompletionCondition, typename ReadHandler>
    inline void async_read(const MutableBufferSequence& buffers,
                           CompletionCondition completion_condition,
                           ReadHandler handler)
    {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            asio::async_read(m_ssl_socket, buffers,
                                    completion_condition, handler);
        else
#endif      
            asio::async_read(m_ssl_socket.next_layer(), buffers,
                                    completion_condition, handler);
    }
    
    /**
     * reads data into the connection's read buffer until completion_condition
     * is met (blocks until finished)
     *
     * @param completion_condition determines if the read operation is complete
     * @param ec contains error code if the read fails
     * @return std::size_t number of bytes read
     *
     * @see asio::read()
     */
    template <typename CompletionCondition>
    inline std::size_t read(CompletionCondition completion_condition,
                            asio::error_code& ec)
    {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            return asio::async_read(m_ssl_socket, asio::buffer(m_read_buffer),
                                           completion_condition, ec);
        else
#endif      
            return asio::async_read(m_ssl_socket.next_layer(), asio::buffer(m_read_buffer),
                                           completion_condition, ec);
    }
    
    /**
     * reads data from the connection until completion_condition is met
     * (blocks until finished)
     *
     * @param buffers one or more buffers into which the data will be read
     * @param completion_condition determines if the read operation is complete
     * @param ec contains error code if the read fails
     * @return std::size_t number of bytes read
     *
     * @see asio::read()
     */
    template <typename MutableBufferSequence, typename CompletionCondition>
    inline std::size_t read(const MutableBufferSequence& buffers,
                            CompletionCondition completion_condition,
                            asio::error_code& ec)
    {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            return asio::read(m_ssl_socket, buffers,
                                     completion_condition, ec);
        else
#endif      
            return asio::read(m_ssl_socket.next_layer(), buffers,
                                     completion_condition, ec);
    }
    
    /**
     * asynchronously writes data to the connection
     *
     * @param buffers one or more buffers containing the data to be written
     * @param handler called after the data has been written
     *
     * @see asio::async_write()
     */
    template <typename ConstBufferSequence, typename write_handler_t>
    inline void async_write(const ConstBufferSequence& buffers, write_handler_t handler) {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            asio::async_write(m_ssl_socket, buffers, handler);
        else
#endif      
            asio::async_write(m_ssl_socket.next_layer(), buffers, handler);
    }   
        
    /**
     * writes data to the connection (blocks until finished)
     *
     * @param buffers one or more buffers containing the data to be written
     * @param ec contains error code if the write fails
     * @return std::size_t number of bytes written
     *
     * @see asio::write()
     */
    template <typename ConstBufferSequence>
    inline std::size_t write(const ConstBufferSequence& buffers,
                             asio::error_code& ec)
    {
#ifdef PION_HAVE_SSL
        if (get_ssl_flag())
            return asio::write(m_ssl_socket, buffers,
                                      asio::transfer_all(), ec);
        else
#endif      
            return asio::write(m_ssl_socket.next_layer(), buffers,
                                      asio::transfer_all(), ec);
    }   
    
    
    /// This function should be called when a server has finished handling
    /// the connection
    inline void finish(void) { if (m_finished_handler) m_finished_handler(shared_from_this()); }

    /// returns true if the connection is encrypted using SSL
    inline bool get_ssl_flag(void) const { return m_ssl_flag; }

    /// sets the lifecycle type for the connection
    inline void set_lifecycle(lifecycle_type t) { m_lifecycle = t; }
    
    /// returns the lifecycle type for the connection
    inline lifecycle_type get_lifecycle(void) const { return m_lifecycle; }
    
    /// returns true if the connection should be kept alive
    inline bool get_keep_alive(void) const { return m_lifecycle != LIFECYCLE_CLOSE; }
    
    /// returns true if the HTTP requests are pipelined
    inline bool get_pipelined(void) const { return m_lifecycle == LIFECYCLE_PIPELINED; }

    /// returns the buffer used for reading data from the TCP connection
    inline read_buffer_type& get_read_buffer(void) { return m_read_buffer; }
    
    /**
     * saves a read position bookmark
     *
     * @param read_ptr points to the next character to be consumed in the read_buffer
     * @param read_end_ptr points to the end of the read_buffer (last byte + 1)
     */
    inline void save_read_pos(const char *read_ptr, const char *read_end_ptr) {
        m_read_position.first = read_ptr;
        m_read_position.second = read_end_ptr;
    }
    
    /**
     * loads a read position bookmark
     *
     * @param read_ptr points to the next character to be consumed in the read_buffer
     * @param read_end_ptr points to the end of the read_buffer (last byte + 1)
     */
    inline void load_read_pos(const char *&read_ptr, const char *&read_end_ptr) const {
        read_ptr = m_read_position.first;
        read_end_ptr = m_read_position.second;
    }

    /// returns an ASIO endpoint for the client connection
    inline asio::ip::tcp::endpoint get_remote_endpoint(void) const {
        asio::ip::tcp::endpoint remote_endpoint;
        try {
            // const_cast is required since lowest_layer() is only defined non-const in asio
            remote_endpoint = const_cast<ssl_socket_type&>(m_ssl_socket).lowest_layer().remote_endpoint();
        } catch (asio::system_error& /* e */) {
            // do nothing
        }
        return remote_endpoint;
    }

    /// returns the client's IP address
    inline asio::ip::address get_remote_ip(void) const {
        return get_remote_endpoint().address();
    }

    /// returns the client's port number
    inline unsigned short get_remote_port(void) const {
        return get_remote_endpoint().port();
    }
    
    /// returns reference to the io_service used for async operations
    inline asio::io_service& get_io_service(void) {
        return m_ssl_socket.lowest_layer().get_io_service();
    }

    /// returns non-const reference to underlying TCP socket object
    inline socket_type& get_socket(void) { return m_ssl_socket.next_layer(); }
    
    /// returns non-const reference to underlying SSL socket object
    inline ssl_socket_type& get_ssl_socket(void) { return m_ssl_socket; }

    /// returns const reference to underlying TCP socket object
    inline const socket_type& get_socket(void) const { return const_cast<ssl_socket_type&>(m_ssl_socket).next_layer(); }
    
    /// returns const reference to underlying SSL socket object
    inline const ssl_socket_type& get_ssl_socket(void) const { return m_ssl_socket; }

    
protected:
        
    /**
     * protected constructor restricts creation of objects (use create())
     *
     * @param io_service asio service associated with the connection
     * @param ssl_context asio ssl context associated with the connection
     * @param ssl_flag if true then the connection will be encrypted using SSL 
     * @param finished_handler function called when a server has finished
     *                         handling the connection
     */
    connection(asio::io_service& io_service,
                  ssl_context_type& ssl_context,
                  const bool ssl_flag,
                  connection_handler finished_handler)
        :
#ifdef PION_HAVE_SSL
        m_ssl_context(asio::ssl::context::sslv23),
        m_ssl_socket(io_service, ssl_context), m_ssl_flag(ssl_flag),
#else
        m_ssl_context(0),
        m_ssl_socket(io_service), m_ssl_flag(false), 
#endif
        m_lifecycle(LIFECYCLE_CLOSE),
        m_finished_handler(finished_handler)
    {
#ifndef PION_HAVE_SSL
        (void) ssl_context;
        (void) ssl_flag;
#endif            
        save_read_pos(NULL, NULL);
    }   
};

/**
 * Data type for a connection pointer
 */
typedef std::shared_ptr<connection> connection_ptr;

} // end namespace tcp
} // end namespace pion

#endif // __PION_TCP_CONNECTION_HEADER__
