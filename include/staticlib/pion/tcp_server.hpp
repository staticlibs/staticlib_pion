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

#ifndef STATICLIB_PION_TCP_SERVER_HPP
#define STATICLIB_PION_TCP_SERVER_HPP

#include <condition_variable>
#include <mutex>
#include <set>

#include "asio.hpp"

#include "staticlib/config.hpp"

#include "staticlib/pion/scheduler.hpp"
#include "staticlib/pion/tcp_connection.hpp"

namespace staticlib { 
namespace pion {

/**
 * Multi-threaded, asynchronous TCP server
 */
class tcp_server {

protected:
    /**
     * Reference to the active scheduler object used to manage worker threads
     */
    scheduler active_scheduler;

    /**
     * Manages async TCP connections
     */
    asio::ip::tcp::acceptor tcp_acceptor;

    /**
     * Context used for SSL configuration
     */
    tcp_connection::ssl_context_type ssl_context;

    /**
     * Condition triggered when the server has stopped listening for connections
     */
    std::condition_variable server_has_stopped;

    /**
     * Condition triggered when the connection pool is empty
     */
    std::condition_variable no_more_connections;

    /**
     * Pool of active connections associated with this server 
     */
    std::set<tcp_connection_ptr> conn_pool;

    /**
     * TCP endpoint used to listen for new connections
     */
    asio::ip::tcp::endpoint tcp_endpoint;

    /**
     * true if the server uses SSL to encrypt connections
     */
    bool ssl_flag;

    /**
     * Set to true when the server is listening for new connections
     */
    bool listening;

    /**
     * Mutex to make class thread-safe
     */
    mutable std::mutex mutex;
    
public:

    /**
     * Protected constructor so that only derived objects may be created
     * 
     * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
     */
    tcp_server(const asio::ip::tcp::endpoint& endpoint, uint32_t number_of_threads) :
    active_scheduler(number_of_threads),
    tcp_acceptor(active_scheduler.get_io_service()),
    ssl_context(asio::ssl::context::sslv23),
    tcp_endpoint(endpoint), 
    ssl_flag(false),
    listening(false) { }

    /**
     * Deleted copy constructor
     */
    tcp_server(const tcp_server&) = delete;

    /**
     * Deleted copy assignment operator
     */
    tcp_server& operator=(const tcp_server&) = delete;

    /**
     * Virtual destructor
     */
    virtual ~tcp_server() STATICLIB_NOEXCEPT {
        if (listening) {
            try {
                stop(false);
            } catch (const std::exception& e) {
                (void) e;
    //            STATICLIB_PION_LOG_WARN("Exception thrown in tcp::server destructor: " << e.message());
            }
        }
    }
    
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
     * Returns true if the server is listening for connections
     * 
     * @return true if the server is listening for connections
     */
    bool is_listening() const {
        return listening;
    }

    /**
     * Handles a new TCP connection; derived classes SHOULD override this
     * since the default behavior does nothing
     * 
     * @param tcp_conn the new TCP connection to handle
     */
    virtual void handle_connection(tcp_connection_ptr& tcp_conn) {
        tcp_conn->set_lifecycle(tcp_connection::lifecycle::close); // make sure it will get closed
        tcp_conn->finish();
    }

    /**
     * Returns an async I/O service used to schedule work
     * 
     * @return asio service
     */
    asio::io_service& get_io_service() {
        return active_scheduler.get_io_service();
    }

    /**
     * Return active scheduler
     * 
     * @return scheduler
     */
    scheduler& get_scheduler() {
        return active_scheduler;
    }

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
    void handle_accept(tcp_connection_ptr& tcp_conn, const std::error_code& accept_error);

    /**
     * Handles new connections following an SSL handshake (checks for errors)
     *
     * @param tcp_conn the new TCP connection (if no error occurred)
     * @param handshake_error true if an error occurred during the SSL handshake
     */
    void handle_ssl_handshake(tcp_connection_ptr& tcp_conn, const std::error_code& handshake_error);

    /**
     * This will be called by connection::finish() after a server has
     * finished handling a connection. If the keep_alive flag is true,
     * it will call handle_connection(); otherwise, it will close the
     * connection and remove it from the server's management pool
     * 
     * @param tcp_conn TCP connection
     */
    void finish_connection(tcp_connection_ptr& tcp_conn);

    /**
     * Prunes orphaned connections that did not close cleanly
     * and returns the remaining number of connections in the pool
     * 
     * @return remaining number of connections in the pool
     */
    std::size_t prune_connections();

};

} // namespace
}

#endif // STATICLIB_PION_TCP_SERVER_HPP
