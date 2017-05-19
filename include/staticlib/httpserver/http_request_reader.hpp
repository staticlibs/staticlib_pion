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

#ifndef STATICLIB_HTTPSERVER_HTTP_REQUEST_READER_HPP
#define STATICLIB_HTTPSERVER_HTTP_REQUEST_READER_HPP

#include <chrono>
#include <functional>
#include <memory>

#include "asio.hpp"

#include "staticlib/config.hpp"
#include "staticlib/concurrent.hpp"

#include "staticlib/httpserver/http_parser.hpp"
#include "staticlib/httpserver/http_request.hpp"
#include "staticlib/httpserver/tcp_connection.hpp"

namespace staticlib { 
namespace httpserver {

/**
 * Asynchronously reads and parses HTTP requests
 */
class http_request_reader : public http_parser, 
        public std::enable_shared_from_this<http_request_reader> {
    
public:

    /**
     * Function called after the HTTP message has been parsed
     */
    using finished_handler_type = std::function<void(http_request_ptr, tcp_connection_ptr&,
            const asio::error_code&)>;

    /**
     * Function called after the HTTP message has been parsed
     */
    using headers_parsing_finished_handler_type = std::function<void(http_request_ptr, 
            tcp_connection_ptr&, const asio::error_code&, sl::support::tribool& rc)>;    
    
private:

    /**
     * Default maximum number of milliseconds for read operations
     */
    static const uint32_t DEFAULT_READ_TIMEOUT_MILLIS;

    /**
     * The HTTP connection that has a new HTTP message to parse
     */
    tcp_connection_ptr m_tcp_conn;

    /**
     * Pointer to a tcp_timer object if read timeouts are enabled
     */
    std::shared_ptr<sl::concurrent::cancelable_timer<asio::steady_timer>> m_timer_ptr;

    /**
     * Maximum number of milliseconds for read operations
     */
    uint32_t m_read_timeout_millis;    
    
    /**
     * The new HTTP message container being created
     */
    http_request_ptr m_http_msg;

    /**
     * Function called after the HTTP message has been parsed
     */
    finished_handler_type m_finished;

    /**
     * Function called after the HTTP message headers have been parsed
     */
    headers_parsing_finished_handler_type m_parsed_headers;    
    
public:

    /**
     * Creates new request_reader objects
     *
     * @param tcp_conn TCP connection containing a new message to parse
     * @param handler function called after the message has been parsed
     */
    static std::shared_ptr<http_request_reader> create(tcp_connection_ptr& tcp_conn, 
            finished_handler_type handler);

    /**
     * Incrementally reads & parses the HTTP message
     */
    void receive();

    /**
     * Returns a shared pointer to the TCP connection
     * 
     * @return shared pointer to the TCP connection
     */
    tcp_connection_ptr& get_connection();

    /**
     * Sets the maximum number of seconds for read operations
     * 
     * @param seconds maximum number of milliseconds for read operations
     */
    void set_timeout(std::chrono::milliseconds timeout);
    
    /**
     * Sets a function to be called after HTTP headers have been parsed
     * 
     * @param h function pointer
     */
    void set_headers_parsed_callback(headers_parsing_finished_handler_type h);
    
private:

    /**
     * Protected constructor restricts creation of objects (use create())
     *
     * @param tcp_conn TCP connection containing a new message to parse
     * @param handler function called after the message has been parsed
     */
    http_request_reader(tcp_connection_ptr& tcp_conn, finished_handler_type handler);

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
    void finished_parsing_headers(const asio::error_code& ec, sl::support::tribool& rc);

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
    http_message& get_message();

};

/**
 * Data type for a request_reader pointer
 */
using reader_ptr = std::shared_ptr<http_request_reader>;

} // namespace
}

#endif // STATICLIB_HTTPSERVER_HTTP_REQUEST_READER_HPP
