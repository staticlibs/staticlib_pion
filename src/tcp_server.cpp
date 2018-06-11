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

#include "staticlib/pion/tcp_server.hpp"

#include <functional>
#include <memory>

#include "asio.hpp"

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/scheduler.hpp"
#include "staticlib/pion/tcp_connection.hpp"

namespace staticlib {
namespace pion {

namespace { // anonymous

const std::string log = "staticlib.pion.tcp_server";

} // namespace

// tcp::server member functions

tcp_server::~tcp_server() STATICLIB_NOEXCEPT {
    if (listening) {
        try {
            stop(false);
        } catch (const std::exception& e) {
            (void) e;
//            STATICLIB_PION_LOG_WARN("Exception thrown in tcp::server destructor: " << e.message());
        }
    }
}

tcp_server::tcp_server(const asio::ip::tcp::endpoint& endpoint, uint32_t number_of_threads) : 
active_scheduler(number_of_threads),
tcp_acceptor(active_scheduler.get_io_service()),
ssl_context(asio::ssl::context::sslv23),
tcp_endpoint(endpoint), 
ssl_flag(false),
listening(false) { }
    
void tcp_server::start() {
    // lock mutex for thread safety
    std::unique_lock<std::mutex> server_lock(mutex);

    if (! listening) {
        STATICLIB_PION_LOG_INFO(log, "Starting server on port " << tcp_endpoint.port());

//        before_starting();

        // configure the acceptor service
        try {
            // get admin permissions in case we're binding to a privileged port
//            admin_rights use_admin_rights(m_endpoint.port() > 0 && m_endpoint.port() < 1024);
            tcp_acceptor.open(tcp_endpoint.protocol());
            // allow the acceptor to reuse the address (i.e. SO_REUSEADDR)
            // ...except when running not on Windows - see http://msdn.microsoft.com/en-us/library/ms740621%28VS.85%29.aspx
#ifndef _MSC_VER
            tcp_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
            tcp_acceptor.bind(tcp_endpoint);
            if (tcp_endpoint.port() == 0) {
                // update the endpoint to reflect the port chosen by bind
                tcp_endpoint = tcp_acceptor.local_endpoint();
            }
            tcp_acceptor.listen();
        } catch (std::exception& e) {
            (void) e;
            STATICLIB_PION_LOG_ERROR(log, "Unable to bind to port " << tcp_endpoint.port() << ": " << e.what());
            throw;
        }

        listening = true;

        // unlock the mutex since listen() requires its own lock
        server_lock.unlock();
        listen();
        
        // notify the thread scheduler that we need it now
        active_scheduler.add_active_user();
    }
}

void tcp_server::stop(bool wait_until_finished) {
    // lock mutex for thread safety
    std::unique_lock<std::mutex> server_lock(mutex);

    if (listening) {
        STATICLIB_PION_LOG_INFO(log, "Shutting down server on port " << tcp_endpoint.port());

        listening = false;

        // this terminates any connections waiting to be accepted
        tcp_acceptor.close();
        
        if (! wait_until_finished) {
            // this terminates any other open connections
            for (tcp_connection_ptr conn : conn_pool) {
                conn->close();
            }
        }

        // wait for all pending connections to complete
        while (! conn_pool.empty()) {
            // try to prun connections that didn't finish cleanly
            if (prune_connections() == 0)
                break;  // if no more left, then we can stop waiting
            // sleep for up to a quarter second to give open connections a chance to finish
            STATICLIB_PION_LOG_INFO(log, "Waiting for open connections to finish");
            scheduler::sleep(no_more_connections, server_lock, 0, 250000000);
        }

        // notify the thread scheduler that we no longer need it
        active_scheduler.remove_active_user();

        // all done!
//        after_stopping();
        server_has_stopped.notify_all();
    }
}

void tcp_server::join(void) {
    std::unique_lock<std::mutex> server_lock(mutex);
    while (listening) {
        // sleep until server_has_stopped condition is signaled
        server_has_stopped.wait(server_lock);
    }
}

void tcp_server::listen() {
    // lock mutex for thread safety
    std::lock_guard<std::mutex> server_lock(mutex);

    if (listening) {
        // create a new TCP connection object
        tcp_connection::connection_handler fc = [this](std::shared_ptr<tcp_connection>& conn) {
            this->finish_connection(conn);
        };
        auto new_connection = std::make_shared<tcp_connection>(
                get_io_service(), ssl_context, ssl_flag, std::move(fc));

        // prune connections that finished uncleanly
        prune_connections();

        // keep track of the object in the server's connection pool
        conn_pool.insert(new_connection);

        // use the object to accept a new connection
        auto cb = [this, new_connection](const std::error_code& ec) mutable {
            this->handle_accept(new_connection, ec);
        };
        new_connection->async_accept(tcp_acceptor, std::move(cb));
    }
}

void tcp_server::handle_accept(tcp_connection_ptr& tcp_conn, const std::error_code& accept_error) {
    if (accept_error) {
        // an error occured while trying to a accept a new connection
        // this happens when the server is being shut down
        if (listening) {
            listen();   // schedule acceptance of another connection
            STATICLIB_PION_LOG_WARN(log, "Accept error on port " << tcp_endpoint.port() << ": " << accept_error.message());
        }
        finish_connection(tcp_conn);
    } else {
        // got a new TCP connection
        STATICLIB_PION_LOG_DEBUG(log, "New" << (tcp_conn->get_ssl_flag() ? " SSL " : " ")
                       << "connection on port " << tcp_endpoint.port());

        // schedule the acceptance of another new connection
        // (this returns immediately since it schedules it as an event)
        if (listening) listen();
        
        // handle the new connection
        if (tcp_conn->get_ssl_flag()) {
            auto cb = [this, tcp_conn](const std::error_code & ec) mutable {
                this->handle_ssl_handshake(tcp_conn, ec);
            };
            tcp_conn->async_handshake_server(std::move(cb));
        } else {
            // not SSL -> call the handler immediately
            handle_connection(tcp_conn);
        }
    }
}

void tcp_server::handle_ssl_handshake(tcp_connection_ptr& tcp_conn,
                                   const std::error_code& handshake_error) {
    if (handshake_error) {
        // an error occured while trying to establish the SSL connection
        STATICLIB_PION_LOG_WARN(log, "SSL handshake failed on port " << tcp_endpoint.port()
                      << " (" << handshake_error.message() << ')');
        finish_connection(tcp_conn);
    } else {
        // handle the new connection
        STATICLIB_PION_LOG_DEBUG(log, "SSL handshake succeeded on port " << tcp_endpoint.port());
        handle_connection(tcp_conn);
    }
}

void tcp_server::finish_connection(tcp_connection_ptr& tcp_conn) {
    std::lock_guard<std::mutex> server_lock(mutex);
    if (listening && tcp_conn->get_keep_alive()) {
        
        // keep the connection alive
        handle_connection(tcp_conn);

    } else {
        STATICLIB_PION_LOG_DEBUG(log, "Closing connection on port " << tcp_endpoint.port());
        
        // remove the connection from the server's management pool
        std::set<tcp_connection_ptr>::iterator conn_itr = conn_pool.find(tcp_conn);
        if (conn_itr != conn_pool.end())
            conn_pool.erase(conn_itr);

        // trigger the no more connections condition if we're waiting to stop
        if (!listening && conn_pool.empty())
            no_more_connections.notify_all();
    }
}

std::size_t tcp_server::prune_connections() {
    // assumes that a server lock has already been acquired
    std::set<tcp_connection_ptr>::iterator conn_itr = conn_pool.begin();
    while (conn_itr != conn_pool.end()) {
        if (conn_itr->unique()) {
            STATICLIB_PION_LOG_WARN(log, "Closing orphaned connection on port " << tcp_endpoint.port());
            std::set<tcp_connection_ptr>::iterator erase_itr = conn_itr;
            ++conn_itr;
            (*erase_itr)->close();
            conn_pool.erase(erase_itr);
        } else {
            ++conn_itr;
        }
    }

    // return the number of connections remaining
    return conn_pool.size();
}

const asio::ip::tcp::endpoint& tcp_server::get_endpoint() const {
    return tcp_endpoint;
}

tcp_connection::ssl_context_type& tcp_server::get_ssl_context_type() {
    return ssl_context;
}

bool tcp_server::is_listening() const {
    return listening;
}

void tcp_server::handle_connection(tcp_connection_ptr& tcp_conn) {
    tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE); // make sure it will get closed
    tcp_conn->finish();
}

asio::io_service& tcp_server::get_io_service() {
    return active_scheduler.get_io_service();
}

scheduler& tcp_server::get_scheduler() {
    return active_scheduler;
}        

} // namespace
}
