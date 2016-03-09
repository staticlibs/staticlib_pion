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

#ifndef STATICLIB_HTTPSERVER_TCP_TIMER_HPP
#define STATICLIB_HTTPSERVER_TCP_TIMER_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>

#include "asio.hpp"

#include "staticlib/httpserver/config.hpp"
#include "staticlib/httpserver/tcp_connection.hpp"

namespace staticlib { 
namespace httpserver {

/**
 * Helper class used to time-out TCP connections
 */
class tcp_timer : public std::enable_shared_from_this<tcp_timer> {
    
    /**
     * Callback handler for the deadline timer
     *
     * @param ec deadline timer error status code
     */
    void timer_callback(const asio::error_code& ec);

    /**
     * Pointer to the TCP connection that is being monitored
     */
    tcp_connection_ptr m_conn_ptr;

    /**
     * Deadline timer used to timeout TCP operations
     */
    asio::steady_timer m_timer;

    /**
     * Mutex used to synchronize the TCP connection timer
     */
    std::mutex m_mutex;

    /**
     * True if the deadline timer is active
     */
    bool m_timer_active;

    /**
     * True if the timer was cancelled (operation completed)
     */
    bool m_was_cancelled;        
    
public:

    /**
     * creates a new TCP connection timer
     *
     * @param conn_ptr pointer to TCP connection to monitor
     */
    tcp_timer(tcp_connection_ptr& conn_ptr);

    /**
     * starts a timer for closing a TCP connection
     *
     * @param seconds number of seconds before the timeout triggers
     */
    void start(const uint32_t seconds);

    /**
     * Cancel the timer (operation completed)
     */
    void cancel(void);

};

/**
 * Shared pointer to a timer object
 */
typedef std::shared_ptr<tcp_timer> tcp_timer_ptr;


} // namespace
}

#endif // STATICLIB_HTTPSERVER_TCP_TIMER_HPP
