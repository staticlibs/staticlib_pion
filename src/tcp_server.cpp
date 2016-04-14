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

#include "staticlib/httpserver/tcp_server.hpp"

#include <functional>
#include <memory>

#include "asio.hpp"

#include "staticlib/httpserver/config.hpp"
#include "staticlib/httpserver/logger.hpp"
#include "staticlib/httpserver/noncopyable.hpp"
#include "staticlib/httpserver/scheduler.hpp"
#include "staticlib/httpserver/tcp_connection.hpp"

namespace staticlib {
namespace httpserver {
    
// tcp::server member functions

tcp_server::~tcp_server() STATICLIB_HTTPSERVER_NOEXCEPT {
    if (m_is_listening) {
        try {
            stop(false);
        } catch (const std::exception& e) {
            (void) e;
//            STATICLIB_HTTPSERVER_LOG_WARN("Exception thrown in tcp::server destructor: " << e.message());
        }
    }
}

tcp_server::tcp_server(scheduler& sched, const unsigned int tcp_port) : 
m_logger(STATICLIB_HTTPSERVER_GET_LOGGER("staticlib.httpserver.tcp_server")),
m_active_scheduler(sched),
m_tcp_acceptor(m_active_scheduler.get_io_service()),
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
m_ssl_context(asio::ssl::context::sslv23),
#else
m_ssl_context(0),
#endif
m_endpoint(asio::ip::tcp::v4(), static_cast<unsigned short>(tcp_port)), 
m_ssl_flag(false), 
m_is_listening(false) { }
    
tcp_server::tcp_server(scheduler& sched, const asio::ip::tcp::endpoint& endpoint) : 
m_logger(STATICLIB_HTTPSERVER_GET_LOGGER("staticlib.httpserver.tcp_server")),
m_active_scheduler(sched),
m_tcp_acceptor(m_active_scheduler.get_io_service()),
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
m_ssl_context(asio::ssl::context::sslv23),
#else
m_ssl_context(0),
#endif
m_endpoint(endpoint), 
m_ssl_flag(false), m_is_listening(false) { }

tcp_server::tcp_server(const unsigned int tcp_port) : 
m_logger(STATICLIB_HTTPSERVER_GET_LOGGER("staticlib.httpserver.tcp_server")),
m_default_scheduler(), 
m_active_scheduler(m_default_scheduler),
m_tcp_acceptor(m_active_scheduler.get_io_service()),
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
m_ssl_context(asio::ssl::context::sslv23),
#else
m_ssl_context(0),
#endif
m_endpoint(asio::ip::tcp::v4(), static_cast<unsigned short>(tcp_port)), 
m_ssl_flag(false), m_is_listening(false) { }

tcp_server::tcp_server(const asio::ip::tcp::endpoint& endpoint) : 
m_logger(STATICLIB_HTTPSERVER_GET_LOGGER("staticlib.httpserver.tcp_server")),
m_default_scheduler(),
m_active_scheduler(m_default_scheduler),
m_tcp_acceptor(m_active_scheduler.get_io_service()),
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
m_ssl_context(asio::ssl::context::sslv23),
#else
m_ssl_context(0),
#endif
m_endpoint(endpoint), 
m_ssl_flag(false),
m_is_listening(false) { }
    
void tcp_server::start() {
    // lock mutex for thread safety
    std::unique_lock<std::mutex> server_lock(m_mutex);

    if (! m_is_listening) {
        STATICLIB_HTTPSERVER_LOG_INFO(m_logger, "Starting server on port " << get_port());
        
        before_starting();

        // configure the acceptor service
        try {
            // get admin permissions in case we're binding to a privileged port
//            admin_rights use_admin_rights(get_port() > 0 && get_port() < 1024);
            m_tcp_acceptor.open(m_endpoint.protocol());
            // allow the acceptor to reuse the address (i.e. SO_REUSEADDR)
            // ...except when running not on Windows - see http://msdn.microsoft.com/en-us/library/ms740621%28VS.85%29.aspx
#ifndef _MSC_VER
            m_tcp_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
            m_tcp_acceptor.bind(m_endpoint);
            if (m_endpoint.port() == 0) {
                // update the endpoint to reflect the port chosen by bind
                m_endpoint = m_tcp_acceptor.local_endpoint();
            }
            m_tcp_acceptor.listen();
        } catch (std::exception& e) {
            (void) e;
            STATICLIB_HTTPSERVER_LOG_ERROR(m_logger, "Unable to bind to port " << get_port() << ": " << e.what());
            throw;
        }

        m_is_listening = true;

        // unlock the mutex since listen() requires its own lock
        server_lock.unlock();
        listen();
        
        // notify the thread scheduler that we need it now
        m_active_scheduler.add_active_user();
    }
}

void tcp_server::stop(bool wait_until_finished) {
    // lock mutex for thread safety
    std::unique_lock<std::mutex> server_lock(m_mutex);

    if (m_is_listening) {
        STATICLIB_HTTPSERVER_LOG_INFO(m_logger, "Shutting down server on port " << get_port());
    
        m_is_listening = false;

        // this terminates any connections waiting to be accepted
        m_tcp_acceptor.close();
        
        if (! wait_until_finished) {
            // this terminates any other open connections
            std::for_each(m_conn_pool.begin(), m_conn_pool.end(),
                          std::bind(&tcp_connection::close, std::placeholders::_1));
        }
    
        // wait for all pending connections to complete
        while (! m_conn_pool.empty()) {
            // try to prun connections that didn't finish cleanly
            if (prune_connections() == 0)
                break;  // if no more left, then we can stop waiting
            // sleep for up to a quarter second to give open connections a chance to finish
            STATICLIB_HTTPSERVER_LOG_INFO(m_logger, "Waiting for open connections to finish");
            scheduler::sleep(m_no_more_connections, server_lock, 0, 250000000);
        }
        
        // notify the thread scheduler that we no longer need it
        m_active_scheduler.remove_active_user();
        
        // all done!
        after_stopping();
        m_server_has_stopped.notify_all();
    }
}

void tcp_server::join(void) {
    std::unique_lock<std::mutex> server_lock(m_mutex);
    while (m_is_listening) {
        // sleep until server_has_stopped condition is signaled
        m_server_has_stopped.wait(server_lock);
    }
}

void tcp_server::set_ssl_key_file(const std::string& pem_key_file) {
    // configure server for SSL
    set_ssl_flag(true);
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
    m_ssl_context.set_options(asio::ssl::context::default_workarounds
                              | asio::ssl::context::no_sslv2
                              | asio::ssl::context::single_dh_use);
    m_ssl_context.use_certificate_file(pem_key_file, asio::ssl::context::pem);
    m_ssl_context.use_private_key_file(pem_key_file, asio::ssl::context::pem);
#else
    (void) pem_key_file;
#endif
}

void tcp_server::listen() {
    // lock mutex for thread safety
    std::lock_guard<std::mutex> server_lock(m_mutex);
    
    if (m_is_listening) {
        // create a new TCP connection object
        tcp_connection_ptr new_connection(tcp_connection::create(get_io_service(),
                                                              m_ssl_context, m_ssl_flag,
                                                              std::bind(&tcp_server::finish_connection, 
                                                              this, std::placeholders::_1)));
        
        // prune connections that finished uncleanly
        prune_connections();

        // keep track of the object in the server's connection pool
        m_conn_pool.insert(new_connection);
        
        // use the object to accept a new connection
        new_connection->async_accept(m_tcp_acceptor,
                                     std::bind(&tcp_server::handle_accept,
                                                 this, new_connection,
                                                 std::placeholders::_1/* asio::placeholders::error */));
    }
}

void tcp_server::handle_accept(tcp_connection_ptr& tcp_conn, const asio::error_code& accept_error) {
    if (accept_error) {
        // an error occured while trying to a accept a new connection
        // this happens when the server is being shut down
        if (m_is_listening) {
            listen();   // schedule acceptance of another connection
            STATICLIB_HTTPSERVER_LOG_WARN(m_logger, "Accept error on port " << get_port() << ": " << accept_error.message());
        }
        finish_connection(tcp_conn);
    } else {
        // got a new TCP connection
        STATICLIB_HTTPSERVER_LOG_DEBUG(m_logger, "New" << (tcp_conn->get_ssl_flag() ? " SSL " : " ")
                       << "connection on port " << get_port());

        // schedule the acceptance of another new connection
        // (this returns immediately since it schedules it as an event)
        if (m_is_listening) listen();
        
        // handle the new connection
#ifdef STATICLIB_HTTPSERVER_HAVE_SSL
        if (tcp_conn->get_ssl_flag()) {
            tcp_conn->async_handshake_server(std::bind(&tcp_server::handle_ssl_handshake,
                                                         this, tcp_conn,
                                                         std::placeholders::_1));
        } else
#endif
            // not SSL -> call the handler immediately
            handle_connection(tcp_conn);
    }
}

void tcp_server::handle_ssl_handshake(tcp_connection_ptr& tcp_conn,
                                   const asio::error_code& handshake_error) {
    if (handshake_error) {
        // an error occured while trying to establish the SSL connection
        STATICLIB_HTTPSERVER_LOG_WARN(m_logger, "SSL handshake failed on port " << get_port()
                      << " (" << handshake_error.message() << ')');
        finish_connection(tcp_conn);
    } else {
        // handle the new connection
        STATICLIB_HTTPSERVER_LOG_DEBUG(m_logger, "SSL handshake succeeded on port " << get_port());
        handle_connection(tcp_conn);
    }
}

void tcp_server::finish_connection(tcp_connection_ptr& tcp_conn) {
    std::lock_guard<std::mutex> server_lock(m_mutex);
    if (m_is_listening && tcp_conn->get_keep_alive()) {
        
        // keep the connection alive
        handle_connection(tcp_conn);

    } else {
        STATICLIB_HTTPSERVER_LOG_DEBUG(m_logger, "Closing connection on port " << get_port());
        
        // remove the connection from the server's management pool
        std::set<tcp_connection_ptr>::iterator conn_itr = m_conn_pool.find(tcp_conn);
        if (conn_itr != m_conn_pool.end())
            m_conn_pool.erase(conn_itr);

        // trigger the no more connections condition if we're waiting to stop
        if (!m_is_listening && m_conn_pool.empty())
            m_no_more_connections.notify_all();
    }
}

std::size_t tcp_server::prune_connections() {
    // assumes that a server lock has already been acquired
    std::set<tcp_connection_ptr>::iterator conn_itr = m_conn_pool.begin();
    while (conn_itr != m_conn_pool.end()) {
        if (conn_itr->unique()) {
            STATICLIB_HTTPSERVER_LOG_WARN(m_logger, "Closing orphaned connection on port " << get_port());
            std::set<tcp_connection_ptr>::iterator erase_itr = conn_itr;
            ++conn_itr;
            (*erase_itr)->close();
            m_conn_pool.erase(erase_itr);
        } else {
            ++conn_itr;
        }
    }

    // return the number of connections remaining
    return m_conn_pool.size();
}

std::size_t tcp_server::get_connections() const {
    std::lock_guard<std::mutex> server_lock(m_mutex);
    return (m_is_listening ? (m_conn_pool.size() - 1) : m_conn_pool.size());
}

unsigned int tcp_server::get_port() const {
    return m_endpoint.port();
}

void tcp_server::set_port(unsigned int p) {
    m_endpoint.port(static_cast<unsigned short> (p));
}

asio::ip::address tcp_server::get_address() const {
    return m_endpoint.address();
}

void tcp_server::set_address(const asio::ip::address& addr) {
    m_endpoint.address(addr);
}

const asio::ip::tcp::endpoint& tcp_server::get_endpoint() const {
    return m_endpoint;
}

void tcp_server::set_endpoint(const asio::ip::tcp::endpoint& ep) {
    m_endpoint = ep;
}

bool tcp_server::get_ssl_flag() const {
    return m_ssl_flag;
}

void tcp_server::set_ssl_flag(bool b) {
    m_ssl_flag = b;
}

tcp_connection::ssl_context_type& tcp_server::get_ssl_context_type() {
    return m_ssl_context;
}

bool tcp_server::is_listening() const {
    return m_is_listening;
}

void tcp_server::set_logger(logger log_ptr) {
    m_logger = log_ptr;
}

logger tcp_server::get_logger() {
    return m_logger;
}

asio::ip::tcp::acceptor& tcp_server::get_acceptor() {
    return m_tcp_acceptor;
}

const asio::ip::tcp::acceptor& tcp_server::get_acceptor() const {
    return m_tcp_acceptor;
}

void tcp_server::handle_connection(tcp_connection_ptr& tcp_conn) {
    tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // make sure it will get closed
    tcp_conn->finish();
}

void tcp_server::before_starting() { }

void tcp_server::after_stopping() { }

asio::io_service& tcp_server::get_io_service() {
    return m_active_scheduler.get_io_service();
}

scheduler& tcp_server::get_active_scheduler() {
    return m_active_scheduler;
}        

} // namespace
}
