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

#ifndef __PION_HTTP_REQUEST_READER_HEADER__
#define __PION_HTTP_REQUEST_READER_HEADER__

#include <functional>
#include <memory>

#include "asio.hpp"

#include "pion/config.hpp"
#include "pion/http/parser.hpp"
#include "pion/http/request.hpp"
#include "pion/tcp/connection.hpp"
#include "pion/tcp/timer.hpp"


namespace pion {
namespace http {

/**
 * Asynchronously reads and parses HTTP requests
 */
class request_reader : public http::parser, public std::enable_shared_from_this<request_reader> {
    
public:

    /**
     * Function called after the HTTP message has been parsed
     */
    typedef std::function<void(http::request_ptr, tcp::connection_ptr&,
            const asio::error_code&)> finished_handler_t;

    /**
     * Function called after the HTTP message has been parsed
     */
    typedef std::function<void(http::request_ptr, tcp::connection_ptr&,
            const asio::error_code&, pion::tribool& rc)> headers_parsing_finished_handler_t;    
    
private:

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
    
    /**
     * The new HTTP message container being created
     */
    http::request_ptr m_http_msg;

    /**
     * Function called after the HTTP message has been parsed
     */
    finished_handler_t m_finished;

    /**
     * Function called after the HTTP message headers have been parsed
     */
    headers_parsing_finished_handler_t m_parsed_headers;    
    
public:

    /**
     * Creates new request_reader objects
     *
     * @param tcp_conn TCP connection containing a new message to parse
     * @param handler function called after the message has been parsed
     */
    static std::shared_ptr<request_reader> create(tcp::connection_ptr& tcp_conn, 
            finished_handler_t handler);

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
    
    /**
     * Sets a function to be called after HTTP headers have been parsed
     * 
     * @param h function pointer
     */
    void set_headers_parsed_callback(headers_parsing_finished_handler_t& h);
    
private:

    /**
     * Protected constructor restricts creation of objects (use create())
     *
     * @param tcp_conn TCP connection containing a new message to parse
     * @param handler function called after the message has been parsed
     */
    request_reader(tcp::connection_ptr& tcp_conn, finished_handler_t handler);

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
     * Reads more bytes for parsing, with timeout support
     */
    void read_bytes_with_timeout();

    /**
     * Handles errors that occur during read operations
     *
     * @param read_error error status from the last read operation
     */
    void handle_read_error(const asio::error_code& read_error);
    
    /**
     * Called after we have finished parsing the HTTP message headers
     * 
     * @param ec error code reference
     * @param rc result code reference
     */
    void finished_parsing_headers(const asio::error_code& ec, pion::tribool& rc);

    /**
     * Reads more bytes from the TCP connection
     */
    void read_bytes();
    
    /**
     * Called after we have finished reading/parsing the HTTP message
     * 
     * @param ec error code reference
     */
    void finished_reading(const asio::error_code& ec);
    
    /**
     * Returns a reference to the HTTP message being parsed
     * 
     * @return HTTP message being parsed
     */
    http::message& get_message();

};

/**
 * Data type for a request_reader pointer
 */
typedef std::shared_ptr<request_reader> reader_ptr;

} // end namespace http
} // end namespace pion

#endif
