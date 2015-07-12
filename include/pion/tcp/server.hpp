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

#ifndef __PION_TCP_SERVER_HEADER__
#define __PION_TCP_SERVER_HEADER__

#include <set>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "asio.hpp"

#include "pion/config.hpp"
#include "pion/noncopyable.hpp"
#include "pion/logger.hpp"
#include "pion/scheduler.hpp"
#include "pion/tcp/connection.hpp"

namespace pion {
namespace tcp {

/**
 * Multi-threaded, asynchronous TCP server
 */
class PION_API server : private pion::noncopyable {

protected:

    /**
     * Primary logging interface used by this class
     */
    logger m_logger;

private:
    
    /**
     * The default scheduler object used to manage worker threads
     */ 
    single_service_scheduler m_default_scheduler;

    /**
     * Reference to the active scheduler object used to manage worker threads
     */
    scheduler & m_active_scheduler;

    /**
     * Manages async TCP connections
     */
    asio::ip::tcp::acceptor m_tcp_acceptor;

protected:
    /**
     * Context used for SSL configuration
     */
    connection::ssl_context_type m_ssl_context;

private:
    /**
     * Condition triggered when the server has stopped listening for connections
     */
    std::condition_variable m_server_has_stopped;

    /**
     * Condition triggered when the connection pool is empty
     */
    std::condition_variable m_no_more_connections;

    /**
     * Pool of active connections associated with this server 
     */
    std::set<tcp::connection_ptr> m_conn_pool;

    /**
     * TCP endpoint used to listen for new connections
     */
    asio::ip::tcp::endpoint m_endpoint;

    /**
     * true if the server uses SSL to encrypt connections
     */
    bool m_ssl_flag;

    /**
     * Set to true when the server is listening for new connections
     */
    bool m_is_listening;

    /**
     * Mutex to make class thread-safe
     */
    mutable std::mutex m_mutex;    
    
public:

    /**
     * Virtual destructor
     */
    virtual ~server() PION_NOEXCEPT;
    
    /**
     * Starts listening for new connections
     */
    void start();

    /**
     * Stops listening for new connections
     *
     * @param wait_until_finished if true, blocks until all pending connections have closed
     */
    void stop(bool wait_until_finished = false);
    
    /**
     * The calling thread will sleep until the server has stopped listening for connections
     */
    void join();

    /**
     * Configures server for SSL using a PEM-encoded RSA private key file
     *
     * @param pem_key_file name of the file containing a PEM-encoded private key
     */
    void set_ssl_key_file(const std::string& pem_key_file);
    
    /**
     * Returns the number of active tcp connections
     * 
     * @return number of active tcp connections
     */
    std::size_t get_connections() const;

    /**
     * Returns tcp port number that the server listens for connections on
     */
    unsigned int get_port() const;
    
    /**
     * Sets tcp port number that the server listens for connections on
     * 
     * @param pport number
     */
    void set_port(unsigned int p);
    
    /**
     * Returns IP address that the server listens for connections on
     * 
     * @return IP address
     */
    asio::ip::address get_address() const;
    
    /**
     * Sets IP address that the server listens for connections on
     * 
     * @param addr IP address
     */
    void set_address(const asio::ip::address& addr);
    
    /**
     * Returns tcp endpoint that the server listens for connections on
     * 
     * @return tcp endpoint
     */
    const asio::ip::tcp::endpoint& get_endpoint() const;
    
    /**
     * Sets tcp endpoint that the server listens for connections on
     * 
     * @param ep tcp endpoint
     */
    void set_endpoint(const asio::ip::tcp::endpoint& ep);

    /**
     * Returns true if the server uses SSL to encrypt connections
     * 
     * @return true if the server uses SSL to encrypt connections
     */
    bool get_ssl_flag() const;
    
    /**
     * Sets value of SSL flag (true if the server uses SSL to encrypt connections)
     * 
     * @param b ssl flag
     */
    void set_ssl_flag(bool b = true);
    
    /**
     * Returns the SSL context for configuration
     * 
     * @return SSL context
     */
    connection::ssl_context_type& get_ssl_context_type();
        
    /**
     * Returns true if the server is listening for connections
     * 
     * @return true if the server is listening for connections
     */
    bool is_listening() const;
    
    /**
     * Sets the logger to be used
     * 
     * @param log_ptr logger
     */
    void set_logger(logger log_ptr);
    
    /**
     * Returns the logger currently in use
     * 
     * @return 
     */
    logger get_logger();
    
    /**
     * Returns mutable reference to the TCP connection acceptor
     * 
     * @return TCP connection acceptor
     */
    asio::ip::tcp::acceptor& get_acceptor();

    /**
     * Returns const reference to the TCP connection acceptor
     * 
     * @return TCP connection acceptor
     */
    const asio::ip::tcp::acceptor& get_acceptor() const;   
        
    /**
     * Protected constructor so that only derived objects may be created
     * 
     * @param tcp_port port number used to listen for new connections (IPv4)
     */
    explicit server(const unsigned int tcp_port);
    
    /**
     * Protected constructor so that only derived objects may be created
     * 
     * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
     */
    explicit server(const asio::ip::tcp::endpoint& endpoint);

    /**
     * Protected constructor so that only derived objects may be created
     * 
     * @param sched the scheduler that will be used to manage worker threads
     * @param tcp_port port number used to listen for new connections (IPv4)
     */
    explicit server(scheduler& sched, const unsigned int tcp_port = 0);
    
    /**
     * Protected constructor so that only derived objects may be created
     * 
     * @param sched the scheduler that will be used to manage worker threads
     * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
     */
    server(scheduler& sched, const asio::ip::tcp::endpoint& endpoint);
    
    /**
     * Handles a new TCP connection; derived classes SHOULD override this
     * since the default behavior does nothing
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp::connection_ptr& tcp_conn);
    
    /**
     * Called before the TCP server starts listening for new connections
     */
    virtual void before_starting();

    /**
     * Called after the TCP server has stopped listing for new connections
     */
    virtual void after_stopping();
    
    /**
     * Returns an async I/O service used to schedule work
     * 
     * @return asio service
     */
    asio::io_service& get_io_service();
    
    /**
     * Return active scheduler
     * 
     * @return scheduler
     */
    scheduler& get_active_scheduler();
    
private:
        
    /**
     * Handles a request to stop the server
     */
    void handle_stop_request();
    
    /**
     * Listens for a new connection
     */
    void listen();

    /**
     * Handles new connections (checks if there was an accept error)
     *
     * @param tcp_conn the new TCP connection (if no error occurred)
     * @param accept_error true if an error occurred while accepting connections
     */
    void handle_accept(tcp::connection_ptr& tcp_conn, const asio::error_code& accept_error);

    /**
     * Handles new connections following an SSL handshake (checks for errors)
     *
     * @param tcp_conn the new TCP connection (if no error occurred)
     * @param handshake_error true if an error occurred during the SSL handshake
     */
    void handle_ssl_handshake(tcp::connection_ptr& tcp_conn, const asio::error_code& handshake_error);
    
    /**
     * This will be called by connection::finish() after a server has
     * finished handling a connection. If the keep_alive flag is true,
     * it will call handle_connection(); otherwise, it will close the
     * connection and remove it from the server's management pool
     * 
     * @param tcp_conn TCP connection
     */
    void finish_connection(tcp::connection_ptr tcp_conn);
    
    /**
     * Prunes orphaned connections that did not close cleanly
     * and returns the remaining number of connections in the pool
     * 
     * @return remaining number of connections in the pool
     */
    std::size_t prune_connections();
    
};

/**
 * Data type for a server pointer
 */
typedef std::shared_ptr<server> server_ptr;

} // end namespace tcp
} // end namespace pion

#endif
