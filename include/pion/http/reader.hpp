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

#ifndef __PION_HTTP_READER_HEADER__
#define __PION_HTTP_READER_HEADER__

#include <cstdint>

#include "asio.hpp"

#include "pion/config.hpp"
#include "pion/http/parser.hpp"
#include "pion/http/message.hpp"
#include "pion/tcp/connection.hpp"
#include "pion/tcp/timer.hpp"


namespace pion {
namespace http {

/**
 * Asynchronously reads and parses HTTP messages
 */
class PION_API reader : public http::parser {

    /**
     * Default maximum number of seconds for read operations
     */
    static const uint32_t DEFAULT_READ_TIMEOUT;

    /**
     * The HTTP connection that has a new HTTP message to parse
     */
    tcp::connection_ptr m_tcp_conn;

    /**
     * Pointer to a tcp::timer object if read timeouts are enabled
     */
    tcp::timer_ptr m_timer_ptr;

    /**
     * Maximum number of seconds for read operations
     */
    uint32_t m_read_timeout;
    
public:
    /**
     * Default destructor
     */
    virtual ~reader();
    
    /**
     * Incrementally reads & parses the HTTP message
     */
    void receive();
    
    /**
     * Returns a shared pointer to the TCP connection
     * 
     * @return shared pointer to the TCP connection
     */
    tcp::connection_ptr& get_connection();
    
    /**
     * Sets the maximum number of seconds for read operations
     * 
     * @param seconds maximum number of seconds for read operations
     */
    void set_timeout(uint32_t seconds);
    
protected:

    /**
     * Protected constructor: only derived classes may create objects
     *
     * @param is_request if true, the message is parsed as an HTTP request;
     *                   if false, the message is parsed as an HTTP response
     * @param tcp_conn TCP connection containing a new message to parse
     */
    reader(const bool is_request, tcp::connection_ptr& tcp_conn);
    
    /**
     * Consumes bytes that have been read using an HTTP parser
     * 
     * @param read_error error status from the last read operation
     * @param bytes_read number of bytes consumed by the last read operation
     */
    void consume_bytes(const asio::error_code& read_error, std::size_t bytes_read);

    /**
     * Consumes bytes that have been read using an HTTP parser
     */
    void consume_bytes();
    
    /**
     * Reads more bytes from the TCP connection
     */
    virtual void read_bytes() = 0;

    /**
     * Called after we have finished reading/parsing the HTTP message
     * 
     * @param ec error code reference
     */
    virtual void finished_reading(const asio::error_code& ec) = 0;

    /**
     * Returns a reference to the HTTP message being parsed
     * 
     * @return reference to the HTTP message being parsed
     */
    virtual http::message& get_message() = 0;

private:

    /**
     * Reads more bytes for parsing, with timeout support
     */
    void read_bytes_with_timeout();

    /**
     * Handles errors that occur during read operations
     *
     * @param read_error error status from the last read operation
     */
    void handle_read_error(const asio::error_code& read_error);

};

} // end namespace http
} // end namespace pion

#endif
