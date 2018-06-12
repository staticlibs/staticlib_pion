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

#ifndef STATICLIB_PION_HTTP_REQUEST_READER_HPP
#define STATICLIB_PION_HTTP_REQUEST_READER_HPP

#include <chrono>
#include <functional>
#include <memory>

#include "asio.hpp"

#include "staticlib/config.hpp"

#include "staticlib/pion/http_parser.hpp"
#include "staticlib/pion/http_request.hpp"
#include "staticlib/pion/tcp_connection.hpp"

namespace staticlib { 
namespace pion {

class http_server;

/**
 * Asynchronously reads and parses HTTP requests
 */
class http_request_reader : public http_parser {
private:
    /**
     * Server reference
     */
    http_server& server;

    /**
     * The HTTP connection that has a new HTTP message to parse
     */
    tcp_connection_ptr tcp_conn;

    /**
     * Maximum number of milliseconds for read operations
     */
    uint32_t read_timeout_millis;

    /**
     * The new HTTP message container being created
     */
    http_request_ptr request;

public:

    /**
     * Constructor to be used with `std::make_unique`
     *
     * @param server reference to server
     * @param tcp_conn TCP connection containing a new message to parse
     */
    http_request_reader(http_server& srv, tcp_connection_ptr& tcp_conn,
            uint32_t read_timeout) :
    http_parser(true),
    server(srv),
    tcp_conn(tcp_conn),
    read_timeout_millis(read_timeout),
    request(new http_request()) {
        request->set_remote_ip(tcp_conn->get_remote_ip());
        request->set_request_reader(this);
    }

    /**
     * Deleted copy-constructor
     * 
     * @param other instance
     */
    http_request_reader(const http_request_reader&) = delete;

    /**
     * Deleted copy-assignment operator
     * 
     * @param other instance
     * @return this instance
     */
    http_request_reader& operator=(const http_request_reader&) = delete;

    /**
     * Incrementally reads & parses the HTTP message
     */
    static void receive(std::unique_ptr<http_request_reader> self);

private:

    /**
     * Consumes bytes that have been read using an HTTP parser
     * 
     * @param read_error error status from the last read operation
     * @param bytes_read number of bytes consumed by the last read operation
     */
    static void consume_bytes(std::unique_ptr<http_request_reader> self, 
            const std::error_code& read_error, std::size_t bytes_read);

    /**
     * Consumes bytes that have been read using an HTTP parser
     * 
     * @param self-owning instance
     */
    static void consume_bytes(std::unique_ptr<http_request_reader> self);
    
    /**
     * Reads more bytes for parsing, with timeout support
     * 
     * @param self-owning instance
     */
    static void read_bytes_with_timeout(std::unique_ptr<http_request_reader> self);

    /**
     * Handles errors that occur during read operations
     *
     * @param read_error error status from the last read operation
     */
    void handle_read_error(const std::error_code& read_error);

    /**
     * Called after we have finished parsing the HTTP message headers
     * 
     * @param ec error code reference
     * @param rc result code reference
     */
    void finished_parsing_headers(const std::error_code& ec, sl::support::tribool& rc);

    /**
     * Called after we have finished reading/parsing the HTTP message
     * 
     * @param ec error code reference
     */
    void finished_reading(const std::error_code& ec);

};

} // namespace
}

#endif // STATICLIB_PION_HTTP_REQUEST_READER_HPP
