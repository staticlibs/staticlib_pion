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

#include "staticlib/httpserver/tcp_timer.hpp"

namespace staticlib { 
namespace httpserver {

// timer member functions

tcp_timer::tcp_timer(tcp_connection_ptr& conn_ptr)
    : m_conn_ptr(conn_ptr), m_timer(conn_ptr->get_io_service()),
    m_timer_active(false), m_was_cancelled(false) { }

void tcp_timer::start(const uint32_t seconds) {
    std::lock_guard<std::mutex> timer_lock(m_mutex);
    m_timer_active = true;
    m_timer.expires_from_now(std::chrono::seconds(seconds));
    auto self = shared_from_this();
    auto cb = [self](const asio::error_code & ec) {
        self->timer_callback(ec);
    };
    m_timer.async_wait(std::move(cb));
}

void tcp_timer::cancel(void) {
    std::lock_guard<std::mutex> timer_lock(m_mutex);
    m_was_cancelled = true;
    if (m_timer_active)
        m_timer.cancel();
}

void tcp_timer::timer_callback(const asio::error_code& /* ec */) {
    std::lock_guard<std::mutex> timer_lock(m_mutex);
    m_timer_active = false;
    if (! m_was_cancelled)
        m_conn_ptr->cancel();
}

} // namespace
}
