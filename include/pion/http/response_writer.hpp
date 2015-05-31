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

#ifndef __PION_HTTP_RESPONSE_WRITER_HEADER__
#define __PION_HTTP_RESPONSE_WRITER_HEADER__

#include <functional>
#include <memory>

#include "asio.hpp"

#include "pion/noncopyable.hpp"
#include "pion/config.hpp"
#include "pion/http/writer.hpp"
#include "pion/http/request.hpp"
#include "pion/http/response.hpp"

namespace pion {
namespace http {

/**
 * Sends HTTP response content asynchronously
 */
class PION_API response_writer : public http::writer, public std::enable_shared_from_this<response_writer> {
    /**
     * The response that will be sent
     */
    http::response_ptr m_http_response;

    /**
     * The initial HTTP response header line
     */
    std::string m_response_line;
    
public:
    
    /**
     * Default destructor
     */
    virtual ~response_writer();

    /**
     * Creates new response_writer objects
     * 
     * @param tcp_conn TCP connection used to send the response
     * @param http_request the request we are responding to
     * @param handler function called after the request has been sent
     * 
     * @return std::shared_ptr<response_writer> shared pointer to
     *         the new writer object that was created
     */
    static std::shared_ptr<response_writer> create(tcp::connection_ptr& tcp_conn, 
            const http::request& http_request, finished_handler_t handler = finished_handler_t());

    /**
     * Returns a non-const reference to the response that will be sent
     * 
     * @return reference to the response that will be sent
     */
    http::response& get_response();
    
    
protected:
    
    /**
     * protected constructor restricts creation of objects (use create())
     * 
     * @param tcp_conn TCP connection used to send the response
     * @param http_response pointer to the response that will be sent
     * @param handler function called after the request has been sent
     */
    response_writer(tcp::connection_ptr& tcp_conn, http::response_ptr& http_response_ptr,
                       finished_handler_t handler);
    
    /**
     * Protected constructor restricts creation of objects (use create())
     * 
     * @param tcp_conn TCP connection used to send the response
     * @param http_request the request we are responding to
     * @param handler function called after the request has been sent
     */
    response_writer(tcp::connection_ptr& tcp_conn, const http::request& http_request,
                       finished_handler_t handler);
    
    /**
     * Initializes a vector of write buffers with the HTTP message information
     *
     * @param write_buffers vector of write buffers to initialize
     */
    virtual void prepare_buffers_for_send(http::message::write_buffers_t& write_buffers);

    /**
     * Returns a function bound to http::writer::handle_write()
     * 
     * @return function bound to http::writer::handle_write()
     */
    virtual write_handler_t bind_to_write_handler();

    /**
     * Called after the response is sent
     * 
     * @param write_error error status from the last write operation
     * @param bytes_written number of bytes sent by the last write operation
     */
    virtual void handle_write(const asio::error_code& write_error, std::size_t bytes_written );
   
};

/**
 * Data type for a response_writer pointer
 */
typedef std::shared_ptr<response_writer> response_writer_ptr;

/**
 * Override operator shift for convenience
 * 
 * @param writer pointer to writer
 * @param data data to write
 * @return pointer to passed writer
 */
template <typename T>
const response_writer_ptr& operator<<(const response_writer_ptr& writer, const T& data) {
    writer->write(data);
    return writer;
}

/**
 * Override operator shift for convenience
 * 
 * @param writer pointer to writer
 * @param iomanip manipulator to write
 * @return pointer to passed writer
 */
inline response_writer_ptr& operator<<(response_writer_ptr& writer, std::ostream& (*iomanip)(std::ostream&)) {
    writer->write(iomanip);
    return writer;
}

}   // end namespace http
}   // end namespace pion

#endif
