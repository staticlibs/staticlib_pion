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

#ifndef STATICLIB_PION_HTTP_RESPONSE_WRITER_HPP
#define STATICLIB_PION_HTTP_RESPONSE_WRITER_HPP

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "asio.hpp"

#include "staticlib/config.hpp"
#include "staticlib/io/span.hpp"

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_message.hpp"
#include "staticlib/pion/http_response.hpp"
#include "staticlib/pion/tcp_connection.hpp"

namespace staticlib { 
namespace pion {

/**
 * Sends HTTP data asynchronously
 */
class http_response_writer {

public:

    /**
     * Function called after the HTTP message has been sent
     */
    using finished_handler_type = std::function<void(const std::error_code&)>;

private:

    /**
     * Data type for a function that handles write operations
     */
    using write_handler_type = std::function<void(const std::error_code&, std::size_t)>;

    /**
     * The HTTP connection that we are writing the message to
     */
    tcp_connection_ptr m_tcp_conn;

    /**
     * I/O write buffers that wrap the payload content to be written
     */
    http_message::write_buffers_type m_content_buffers;

    /**
     * Caches binary data included within the payload content
     */
    std::vector<std::unique_ptr<char[]>> payload_cache;

    /**
     * The length (in bytes) of the response content to be sent (Content-Length)
     */
    size_t m_content_length;

    /**
     * True if the HTTP client supports chunked transfer encodings
     */
    bool m_client_supports_chunks;

    /**
     * True if data is being sent to the client using multiple chunks
     */
    bool m_sending_chunks;

    /**
     * True if the HTTP message headers have already been sent
     */
    bool m_sent_headers;

    /**
     * Function called after the HTTP message has been sent
     */
    finished_handler_type m_finished;

    /**
     * The response that will be sent
     */
    std::unique_ptr<http_response> m_http_response;

    /**
     * The initial HTTP response header line
     */
    std::string m_response_line;

public:

    /**
     * Constructor to be used with `std::make_unique`
     * 
     * @param tcp_conn TCP connection used to send the response
     * @param http_request the request we are responding to
     */
    http_response_writer(tcp_connection_ptr& tcp_conn, const http_request& http_request);

    /**
     * Deleted copy constructor
     */
    http_response_writer(const http_response_writer&) = delete;

    /**
     * Deleted copy assignment operator
     */
    http_response_writer& operator=(const http_response_writer&) = delete;

    /**
     * Returns a non-const reference to the response that will be sent
     * 
     * @return reference to the response that will be sent
     */
    http_response& get_response();

    /**
     * Clears out all of the memory buffers used to cache payload content data
     */
    void clear();

    /**
     * Write payload content
     *
     * @param data to append to the payload content
     */
    void write(sl::io::span<const char> data);

    /**
     * Write payload content; the data written is not
     * copied, and therefore must persist until the message has finished
     * sending
     *
     * @param data the data to append to the payload content
     */
    void write_nocopy(sl::io::span<const char> data);

    /**
     * Sends all data buffered as a single HTTP message (without chunking).
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object.
     */
    static void send(std::unique_ptr<http_response_writer> self);

    /**
     * Sends all data buffered as a single HTTP chunk.  Following a call to this
     * function, it is not thread safe to use your reference to the writer
     * object until the send_handler has been called.
     * 
     * @param send_handler function that is called after the chunk has been sent
     *                     to the client.  Your callback function must end by
     *                     calling one of send_chunk() or send_final_chunk().  Also,
     *                     be sure to clear() the writer before writing data to it.
     */
    template <typename SendHandler> void send_chunk(SendHandler send_handler) {
        m_sending_chunks = true;
        if (!supports_chunked_messages()) {
            // sending data in chunks, but the client does not support chunking;
            // make sure that the connection will be closed when we are all done
            m_tcp_conn->set_lifecycle(tcp_connection::LIFECYCLE_CLOSE);
        }
        // make sure that we did not lose the TCP connection
        if (m_tcp_conn->is_open()) {
            // send more data
            send_more_data(false, send_handler);
        } else {
            finished_writing(asio::error::connection_reset);
        }
    }

    /**
     * Sends all data buffered (if any) and also sends the final HTTP chunk.
     * This function (either overloaded version) must be called following any 
     * calls to send_chunk().
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object.
     */ 
    static void send_final_chunk(std::unique_ptr<http_response_writer> self);

    /**
     * Returns a shared pointer to the TCP connection
     * 
     * @return shared pointer to the TCP connection
     */
    tcp_connection_ptr& get_connection();

    /**
     * Returns the length of the payload content (in bytes)
     * 
     * @return length of the payload content (in bytes)
     */
    size_t get_content_length() const;

    /**
     * Sets whether or not the client supports chunked messages
     * 
     * @param b whether or not the client supports chunked messages
     */
    void supports_chunked_messages(bool b);

    /**
     * Returns true if the client supports chunked messages
     * 
     * @return true if the client supports chunked messages
     */
    bool supports_chunked_messages() const;

    /**
     * Returns true if we are sending a chunked message to the client
     * 
     * @return true if we are sending a chunked message to the client
     */
    bool sending_chunked_message() const;

private:

    /**
     * called after we have finished sending the HTTP message
     * 
     * @param ec
     */
    void finished_writing(const std::error_code& ec);

    /**
     * Initializes a vector of write buffers with the HTTP message information
     *
     * @param write_buffers vector of write buffers to initialize
     */
    void prepare_buffers_for_send(http_message::write_buffers_type& write_buffers);

    /**
     * Returns a function bound to http::writer::handle_write()
     * 
     * @return function bound to http::writer::handle_write()
     */
    static write_handler_type bind_to_write_handler(std::unique_ptr<http_response_writer> self);

    /**
     * Called after the response is sent
     * 
     * @param write_error error status from the last write operation
     * @param bytes_written number of bytes sent by the last write operation
     */
    static void handle_write(std::unique_ptr<http_response_writer> self,
            const std::error_code& write_error, std::size_t bytes_written);

    /**
     * Sends all of the buffered data to the client
     *
     * @param send_final_chunk true if the final 0-byte chunk should be included
     * @param send_handler function called after the data has been sent
     */
    template <typename SendHandler>
    void send_more_data(const bool send_final_chunk, SendHandler send_handler) {
        // make sure that we did not lose the TCP connection
        if (m_tcp_conn->is_open()) {
            // make sure that the content-length is up-to-date
            // prepare the write buffers to be sent
            http_message::write_buffers_type write_buffers;
            prepare_write_buffers(write_buffers, send_final_chunk);
            // send data in the write buffers
            auto send_handler_stranded = m_tcp_conn->get_strand().wrap(send_handler);
            m_tcp_conn->async_write(write_buffers, send_handler_stranded);
        } else {
            finished_writing(asio::error::connection_reset);
        }
    }

    /**
     * Prepares write_buffers for next send operation
     *
     * @param write_buffers buffers to which data will be appended
     * @param send_final_chunk true if the final 0-byte chunk should be included
     */
    void prepare_write_buffers(http_message::write_buffers_type &write_buffers, 
            const bool send_final_chunk);

    /**
     * Add data to cache
     * 
     * @param data data span
     * @return asio buffer pointing to copy of passed data
     */
    asio::const_buffer add_to_cache(sl::io::span<const char> data);
};

/**
 * Data type for a response_writer pointer
 */
using response_writer_ptr = std::unique_ptr<http_response_writer>;

} // namespace
}

#endif // STATICLIB_PION_HTTP_RESPONSE_WRITER_HPP
