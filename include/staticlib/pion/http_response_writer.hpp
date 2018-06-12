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
    /**
     * The HTTP connection that we are writing the message to
     */
    tcp_connection_ptr tcp_conn;

    /**
     * I/O write buffers that wrap the payload content to be written
     */
    http_message::write_buffers_type content_buffers;

    /**
     * Caches binary data included within the payload content
     */
    std::vector<std::unique_ptr<char[]>> payload_cache;

    /**
     * The length (in bytes) of the response content to be sent (Content-Length)
     */
    size_t content_length;

    /**
     * True if the HTTP client supports chunked transfer encodings
     */
    bool client_supports_chunks;

    /**
     * True if data is being sent to the client using multiple chunks
     */
    bool sending_chunks;

    /**
     * True if the HTTP message headers have already been sent
     */
    bool sent_headers;

    /**
     * The response that will be sent
     */
    std::unique_ptr<http_response> response;

    /**
     * The initial HTTP response header line
     */
    std::string response_line;

public:

    /**
     * Constructor to be used with `std::make_unique`
     * 
     * @param tcp_conn TCP connection used to send the response
     * @param http_request the request we are responding to
     */
    http_response_writer(tcp_connection_ptr& tcp_conn, const http_request& http_request) :
    tcp_conn(tcp_conn),
    content_length(0),
    client_supports_chunks(true),
    sending_chunks(false),
    sent_headers(false),
    response(new http_response(http_request)) {
        // set whether or not the client supports chunks
        supports_chunked_messages(response->get_chunks_supported());
    }

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
    http_response& get_response() {
        return *response;
    }

    /**
     * Clears out all of the memory buffers used to cache payload content data
     */
    void clear() {
        content_buffers.clear();
        payload_cache.clear();
        content_length = 0;
    }

    /**
     * Write payload content
     *
     * @param data to append to the payload content
     */
    void write(sl::io::span<const char> data) {
        if (response->is_body_allowed() && data.size() > 0) {
            content_buffers.push_back(add_to_cache(data));
            content_length += data.size();
        }
    }

    /**
     * Write payload content; the data written is not
     * copied, and therefore must persist until the message has finished
     * sending
     *
     * @param data the data to append to the payload content
     */
    void write_nocopy(sl::io::span<const char> data) {
        if (response->is_body_allowed() && data.size() > 0) {
            content_buffers.push_back(asio::buffer(data.data(), data.size()));
            content_length += data.size();
        }
    }

    /**
     * Sends all data buffered as a single HTTP message (without chunking).
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object.
     * 
     * @param self-owning instance
     */
    static void send(std::unique_ptr<http_response_writer> self) {
        auto self_ptr = self.get();
        auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
        self_ptr->send_more_data(false,
            [self_shared](const std::error_code& ec, std::size_t bt) { 
                auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                if (nullptr != self.get()) {
                    handle_write(std::move(self), ec, bt); 
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.http_response_writer",
                            "Lost context detected in 'async_write'");
                }
            });
    }

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
        sending_chunks = true;
        if (!supports_chunked_messages()) {
            // sending data in chunks, but the client does not support chunking;
            // make sure that the connection will be closed when we are all done
            tcp_conn->set_lifecycle(tcp_connection::lifecycle::close);
        }
        // make sure that we did not lose the TCP connection
        if (tcp_conn->is_open()) {
            // send more data
            send_more_data(false, send_handler);
        } else {
            tcp_conn->finish();
        }
    }

    /**
     * Sends all data buffered (if any) and also sends the final HTTP chunk.
     * This function (either overloaded version) must be called following any 
     * calls to send_chunk().
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object.
     * 
     * @param self-owning instance
     */ 
    static void send_final_chunk(std::unique_ptr<http_response_writer> self) {
        self->sending_chunks = true;
        auto self_ptr = self.get();
        auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
        self_ptr->send_more_data(true,
            [self_shared](const std::error_code& ec, std::size_t bt) { 
                auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                if (nullptr != self.get()) {
                    handle_write(std::move(self), ec, bt); 
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.http_response_writer",
                            "Lost context detected in 'async_write' final");
                }
            });
    }

    /**
     * Returns a shared pointer to the TCP connection
     * 
     * @return shared pointer to the TCP connection
     */
    tcp_connection_ptr& get_connection() {
        return tcp_conn;
    }

    /**
     * Returns the length of the payload content (in bytes)
     * 
     * @return length of the payload content (in bytes)
     */
    size_t get_content_length() const {
        return content_length;
    }

    /**
     * Sets whether or not the client supports chunked messages
     * 
     * @param b whether or not the client supports chunked messages
     */
    void supports_chunked_messages(bool b) {
        client_supports_chunks = b;
    }

    /**
     * Returns true if the client supports chunked messages
     * 
     * @return true if the client supports chunked messages
     */
    bool supports_chunked_messages() const {
        return client_supports_chunks;
    }

    /**
     * Returns true if we are sending a chunked message to the client
     * 
     * @return true if we are sending a chunked message to the client
     */
    bool sending_chunked_message() const {
        return sending_chunks;
    }

private:

    /**
     * Initializes a vector of write buffers with the HTTP message information
     *
     * @param write_buffers vector of write buffers to initialize
     */
    void prepare_buffers_for_send(http_message::write_buffers_type& write_buffers) {
        if (get_content_length() > 0) {
            response->set_content_length(get_content_length());
        }
        response->prepare_buffers_for_send(write_buffers, get_connection()->get_keep_alive(),
                sending_chunked_message());
    }

    /**
     * Called after the response is sent
     * 
     * @param self-owning instance
     * @param write_error error status from the last write operation
     * @param bytes_written number of bytes sent by the last write operation
     */
    static void handle_write(std::unique_ptr<http_response_writer> self,
            const std::error_code& ec, std::size_t bytes_written) {
        if (!ec) {
            // response sent OK
            if (self->sending_chunked_message()) {
                STATICLIB_PION_LOG_DEBUG("staticlib.pion.http_response_writer",
                        "Sent HTTP response chunk of " << bytes_written << " bytes");
            } else {
                STATICLIB_PION_LOG_DEBUG("staticlib.pion.http_response_writer",
                        "Sent HTTP response of " << bytes_written << " bytes ("
                        << (self->get_connection()->get_keep_alive() ? "keeping alive)" : "closing)"));
            }
        }
        self->tcp_conn->finish();
    }

    /**
     * Sends all of the buffered data to the client
     *
     * @param send_final_chunk true if the final 0-byte chunk should be included
     * @param send_handler function called after the data has been sent
     */
    template <typename SendHandler>
    void send_more_data(const bool send_final_chunk, SendHandler send_handler) {
        // make sure that we did not lose the TCP connection
        if (tcp_conn->is_open()) {
            // make sure that the content-length is up-to-date
            // prepare the write buffers to be sent
            http_message::write_buffers_type write_buffers;
            prepare_write_buffers(write_buffers, send_final_chunk);
            // send data in the write buffers
            auto send_handler_stranded = tcp_conn->get_strand().wrap(send_handler);
            tcp_conn->async_write(write_buffers, send_handler_stranded);
        } else {
            tcp_conn->finish();
        }
    }

    /**
     * Prepares write_buffers for next send operation
     *
     * @param write_buffers buffers to which data will be appended
     * @param send_final_chunk true if the final 0-byte chunk should be included
     */
    void prepare_write_buffers(http_message::write_buffers_type &write_buffers, 
            const bool send_final_chunk) {
        // check if the HTTP headers have been sent yet
        if (! sent_headers) {
            // initialize write buffers for send operation
            prepare_buffers_for_send(write_buffers);

            // only send the headers once
            sent_headers = true;
        }

        // combine I/O write buffers (headers and content) so that everything
        // can be sent together; otherwise, we would have to send headers
        // and content separately, which would not be as efficient

        // don't send anything if there is no data in content buffers
        if (content_length > 0) {
            if (supports_chunked_messages() && sending_chunked_message()) {
                // prepare the next chunk of data to send
                // write chunk length in hex
                auto cast_buf = std::array<char, 35>();
                auto written = sprintf(cast_buf.data(), "%lx", static_cast<long>(content_length));

                // add chunk length as a string at the back of the text cache
                // append length of chunk to write_buffers
                write_buffers.push_back(add_to_cache({cast_buf.data(), written}));
                // append an extra CRLF for chunk formatting
                write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));

                // append response content buffers
                write_buffers.insert(write_buffers.end(), content_buffers.begin(),
                                     content_buffers.end());
                // append an extra CRLF for chunk formatting
                write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));
            } else {
                // append response content buffers
                write_buffers.insert(write_buffers.end(), content_buffers.begin(),
                                     content_buffers.end());
            }
        }

        // prepare a zero-byte (final) chunk
        if (send_final_chunk && supports_chunked_messages() && sending_chunked_message()) {
            // add chunk length as a string at the back of the text cache
            char zero = '0';
            // append length of chunk to write_buffers
            write_buffers.push_back(add_to_cache({std::addressof(zero), 1}));
            // append an extra CRLF for chunk formatting
            write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));
            write_buffers.push_back(asio::buffer(http_message::STRING_CRLF));
        }
    }

    /**
     * Add data to cache
     * 
     * @param data data span
     * @return asio buffer pointing to copy of passed data
     */
    asio::const_buffer add_to_cache(sl::io::span<const char> data) {
        payload_cache.emplace_back(new char[data.size()]);
        char* dest = payload_cache.back().get();
        std::memcpy(dest, data.data(), data.size());
        return asio::buffer(dest, data.size());
    }
};

/**
 * Data type for a response_writer pointer
 */
using response_writer_ptr = std::unique_ptr<http_response_writer>;

} // namespace
}

#endif // STATICLIB_PION_HTTP_RESPONSE_WRITER_HPP
