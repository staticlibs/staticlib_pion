/*
 * Copyright 2018, alex at staticlibs.net
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

/* 
 * File:   websocket.hpp
 * Author: alex
 *
 * Created on June 16, 2018, 5:57 PM
 */

#ifndef STATICLIB_PION_WEBSOCKET_HPP
#define STATICLIB_PION_WEBSOCKET_HPP

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "asio.hpp"

#include "staticlib/support.hpp"
#include "staticlib/utils.hpp"
#include "staticlib/websocket.hpp"

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_request.hpp"
#include "staticlib/pion/tcp_connection.hpp"

namespace staticlib { 
namespace pion {

// forward declaration
class websocket;

/**
 * Type of function that is used to handle WebSocket connections
 */
using websocket_handler_type = std::function<void(std::unique_ptr<websocket>, sl::websocket::frame)>;


class websocket {
    std::unique_ptr<http_request> request;
    std::shared_ptr<tcp_connection> connection;

    websocket_handler_type on_open_fun;
    websocket_handler_type on_message_fun;
    websocket_handler_type on_close_fun;

    /**
     * I/O write buffers that wrap the payload content to be written
     */
    std::vector<asio::const_buffer> payload_buffers;

    /**
     * Caches binary data included within the payload content
     */
    std::vector<std::unique_ptr<char[]>> payload_cache;

    size_t payload_length = 0;

    std::vector<char> receive_buffer;
    std::vector<std::unique_ptr<char[]>> receive_partials;

public:
    websocket(std::unique_ptr<http_request> req, std::shared_ptr<tcp_connection> conn,
            websocket_handler_type open_handler, websocket_handler_type message_handler,
            websocket_handler_type close_handler) :
    request(std::move(req)),
    connection(std::move(conn)),
    on_open_fun(std::move(open_handler)),
    on_message_fun(std::move(message_handler)),
    on_close_fun(std::move(close_handler)) {
        connection->set_lifecycle(tcp_connection::lifecycle::close);
    }

    ~websocket() STATICLIB_NOEXCEPT {
        try {
            connection->finish();
        } catch(...) {
            // no-op
        }
    }

    websocket(const websocket&) = delete;

    websocket& operator=(const websocket&) = delete;

    const http_request& get_request() const {
        return *request;
    }

    /**
     * Write payload content
     *
     * @param data to append to the payload content
     */
    void write(sl::io::span<const char> data) {
        if (data.size() > 0) {
            payload_buffers.push_back(add_to_payload_cache(data));
            payload_length += data.size();
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
        if (data.size() > 0) {
            payload_buffers.push_back(asio::buffer(data.data(), data.size()));
            payload_length += data.size();
        }
    }

    static bool is_websocket_upgrade(const http_request& req) {
        return sl::utils::iequals(req.get_header("Upgrade"), "websocket") &&
                sl::utils::iequals(req.get_header("Sec-WebSocket-Version"), "13") &&
                sl::utils::iequals(req.get_header("Connection"), "Upgrade") &&
                !req.get_header("Host").empty() &&
                !req.get_header("Sec-WebSocket-Key").empty();
    }

    static void start(std::unique_ptr<websocket> self) {
        self->prepare_handshake();
        send_internal(std::move(self),
            [](std::unique_ptr<websocket> self) {
                on_open(std::move(self));
            });
    }

    static void receive(std::unique_ptr<websocket> self) {
        self->clear_receive();
        receive_internal(std::move(self));
    }

    static void send(std::unique_ptr<websocket> self,
            sl::websocket::frame_type msg_type = sl::websocket::frame_type::text) {
        send_message(std::move(self), msg_type, websocket::receive);
    }

    template<typename Handler>
    static void send_message(std::unique_ptr<websocket> self, sl::websocket::frame_type msg_type,
            Handler handler) {
        self->prepare_header(msg_type);
        // safety precaution for delayed responses
        post_with_strand(std::move(self),
            [handler](std::unique_ptr<websocket> self) {
                send_internal(std::move(self), std::move(handler));
            });
    }

    template<typename Range>
    static void broadcast(Range& connections, sl::io::span<const char> msg,
            sl::websocket::frame_type msg_type = sl::websocket::frame_type::text) {
        // prepare message
        auto holder = std::make_shared<std::vector<char>>();
        auto hbuf = std::array<char, 10>();
        auto header = sl::websocket::frame::make_header(hbuf, msg_type, msg.size());
        holder->resize(header.size() + msg.size());
        std::memcpy(holder->data(), header.data(), header.size());
        std::memcpy(holder->data() + header.size(), msg.data(), msg.size());
        
        // broadcast message
        for (auto it = connections.first; it != connections.second; ++it) {
            auto conn_shared = it->second.lock();
            if (nullptr != conn_shared.get()) {
                send_broadcast(std::move(conn_shared), holder);
            }
        }
    }

private:

    /**
     * Add data to payload cache
     *
     * @param data data span
     * @return asio buffer pointing to copy of passed data
     */
    asio::const_buffer add_to_payload_cache(sl::io::span<const char> buf) {
        payload_cache.emplace_back(new char[buf.size()]);
        char* dest = payload_cache.back().get();
        std::memcpy(dest, buf.data(), buf.size());
        return asio::buffer(dest, buf.size());
    }

    void add_to_receive_buffer(sl::io::span<const char> buf) {
        auto size = receive_buffer.size();
        receive_buffer.resize(size + buf.size());
        std::memcpy(receive_buffer.data() + size, buf.data(), buf.size());
    }

    void add_to_receive_partials(sl::io::span<const char> payload) {
        receive_partials.emplace_back(new char[payload.size()]);
        char* dest = payload_cache.back().get();
        std::memcpy(dest, payload.data(), payload.size());
        receive_buffer.clear();
    }

    void clear_payload() {
        payload_buffers.clear();
        payload_buffers.push_back(asio::buffer(sl::utils::empty_string().data(), 0));
        payload_cache.clear();
        payload_length = 0;
    }

    void clear_receive() {
        receive_buffer.clear();
        receive_partials.clear();
    }

    void prepare_header(sl::websocket::frame_type msg_type) {
        auto buf = std::array<char, 10>();
        auto header = sl::websocket::frame::make_header(buf, msg_type, payload_length);
        payload_cache.emplace_back(new char[header.size()]);
        char* dest = payload_cache.back().get();
        std::memcpy(dest, header.data(), header.size());
        payload_buffers[0] = asio::buffer(dest, header.size());
    }

    void prepare_handshake() {
        write(sl::websocket::handshake::make_response_line());
        write_nocopy("\r\n");
        auto& key = request->get_header("Sec-WebSocket-Key");
        auto headers = sl::websocket::handshake::make_response_headers(key);
        for (auto& ha : headers) {
            write(ha.first);
            write_nocopy(": ");
            write(ha.second);
            write_nocopy("\r\n");
        }
        write_nocopy("\r\n");
    }

    template<typename Handler>
    static void post_with_strand(std::unique_ptr<websocket> self, Handler handler) {
        auto self_ptr = self.get();
        auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
        auto post_handler =
            [self_shared, handler]() {
                auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                if (nullptr != self.get()) {
                    handler(std::move(self));
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket",
                            "Lost context detected in 'post'");
                }
            };
        self_ptr->connection->get_strand().post(std::move(post_handler));
    }

    template<typename Handler>
    static void send_internal(std::unique_ptr<websocket> self, Handler handler) {
        auto self_ptr = self.get();
        auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
        auto send_handler =
            [self_shared, handler](const asio::error_code& ec, size_t) {
                auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                if (nullptr != self.get()) {
                    self->clear_payload();
                    if (!ec) {
                        handler(std::move(self));
                    } else {
                        on_close(std::move(self));
                        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket",
                                "Write error, code: [" << ec <<"]");
                    }
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket",
                            "Lost context detected in 'async_write'");
                }
            };
        auto send_handler_stranded = self_ptr->connection->get_strand().wrap(send_handler);
        self_ptr->connection->async_write(self_ptr->payload_buffers, std::move(send_handler_stranded));
    }

    static void receive_internal(std::unique_ptr<websocket> self) {
        auto self_ptr = self.get();
        auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
        auto read_handler =
            [self_shared](const std::error_code& ec, size_t bytes_read) {
                auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                if (nullptr != self.get()) {
                    if (!ec) {
                        const char* dptr = self->connection->get_read_buffer().data();
                        auto buf = sl::io::make_span(dptr, bytes_read);
                        consume(std::move(self), buf);
                    } else {
                        on_close(std::move(self));
                        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket",
                                "Read error, code: [" << ec <<"]");
                    }
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket",
                            "Lost context detected in 'async_read_some'");
                }
            };
        auto read_handler_stranded = self_ptr->connection->get_strand().wrap(std::move(read_handler));
        self_ptr->connection->async_read_some(std::move(read_handler_stranded));
    }

    static void send_broadcast(std::shared_ptr<tcp_connection> conn,
            std::shared_ptr<std::vector<char>> data) {
        auto handler =
            [conn, data]() {
                conn->async_write(asio::buffer(data->data(), data->size()),
                    [data](const std::error_code&, size_t){ /* no-op */ });
            };
        conn->get_strand().post(std::move(handler));
    }

    static void consume(std::unique_ptr<websocket> self, sl::io::span<const char> buf) {
        auto frame = sl::websocket::frame(buf);
        if (!(frame.is_well_formed() && frame.is_complete())) {
            self->add_to_receive_buffer(buf);
            frame = sl::websocket::frame(self->receive_buffer_span());
        }
        if (!frame.is_well_formed()) {
            STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket",
                    "Invalid frame received, header: [" << frame.header_hex() << "]");
            on_close(std::move(self));
            return;
        }
        if (!frame.is_complete()) {
            receive_internal(std::move(self));
            return;
        }
        if (!frame.is_final()) {
            on_continuation(std::move(self), frame);
            return;
        }
        // todo: check partials: generic list source in sl::io
        switch (frame.type()) {
        case sl::websocket::frame_type::text:
        case sl::websocket::frame_type::binary:
            on_message(std::move(self), frame);
            return;
        case sl::websocket::frame_type::ping:
            on_ping(std::move(self), frame);
            return;
        case sl::websocket::frame_type::continuation:
            on_continuation(std::move(self), frame);
            return;
        case sl::websocket::frame_type::close:
        default:
            on_close(std::move(self));
            STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "Closing WebSocket connection");
        };
    }

    static void on_open(std::unique_ptr<websocket> self) {
        auto self_ptr = self.get();
        self_ptr->on_open_fun(std::move(self), sl::websocket::frame({nullptr, 0}));
    }

    static void on_message(std::unique_ptr<websocket> self, sl::websocket::frame frame) {
        auto self_ptr = self.get();
        self_ptr->on_message_fun(std::move(self), frame);
    }

    static void on_close(std::unique_ptr<websocket> self) {
        auto self_ptr = self.get();
        self_ptr->on_close_fun(std::move(self), sl::websocket::frame({nullptr, 0}));
    }

    static void on_ping(std::unique_ptr<websocket> self, sl::websocket::frame frame) {
        if (frame.payload_length() > 0) {
            auto src = frame.payload_unmasked();
            auto buf = std::array<char, (1<<7)>();
            auto len = src.read({buf.data(), buf.size()});
            self->write({buf.data(), len});
        }
        send_internal(std::move(self),
            [](std::unique_ptr<websocket> self) {
                receive(std::move(self));
            });
    }

    static void on_continuation(std::unique_ptr<websocket> self, sl::websocket::frame frame) {
        self->add_to_receive_partials(frame.raw_view());
        receive_internal(std::move(self));
    }

    sl::io::span<const char> receive_buffer_span() {
        return sl::io::make_span(
                const_cast<const char*>(receive_buffer.data()), 
                receive_buffer.size());
    }
};

/**
 * Data type for a WebSocket connection pointer
 */
using websocket_ptr = std::unique_ptr<websocket>;

} // namespace
}

#endif /* STATICLIB_PION_WEBSOCKET_HPP */

