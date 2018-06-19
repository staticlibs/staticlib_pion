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

namespace websocket_detail {

class msg_data_src {
    std::vector<sl::websocket::frame>& frames_ref;
    sl::websocket::masked_payload_source src_single;
    sl::io::multi_source<std::vector<sl::websocket::masked_payload_source>> src_multi;
    size_t frames_size;

public:
    msg_data_src(std::vector<sl::websocket::frame>& frames):
    frames_ref(frames),
    src_single(
        [&frames] {
            if (1 == frames.size()) {
                return frames[0].payload_unmasked();
            } else {
                return sl::websocket::masked_payload_source({nullptr, 0}, 0);
            }
        } ()),
    src_multi(
        [&frames] {
            auto vec = std::vector<sl::websocket::masked_payload_source>();
            if (frames.size() > 1) {
                for (auto& fr : frames) {
                    vec.emplace_back(fr.payload_unmasked());
                }
            } 
            return sl::io::make_multi_source(std::move(vec));
        } ()) { }

    msg_data_src(const msg_data_src&) = delete;

    msg_data_src& operator=(const msg_data_src&) = delete;

    msg_data_src(msg_data_src&& other):
    frames_ref(other.frames_ref),
    src_single(std::move(other.src_single)),
    src_multi(std::move(other.src_multi)) { }

    msg_data_src& operator=(msg_data_src&&) = delete;

    std::streamsize read(sl::io::span<char> span) {
        if (1 == frames_ref.size()) {
            return src_single.read(span);
        } else if (frames_ref.size() > 1) {
            return src_multi.read(span);
        } else {
            return std::char_traits<char>::eof();
        }
    }
};

} // namespace


class websocket {

    enum class close_status : uint32_t {
        normal = 0xe8030288,
        going_away = 0xe9030288,
        protocol_error = 0xea030288,
        overflow = 0xf1030288,
        error = 0xf3030288
    };

    // IO
    std::unique_ptr<http_request> request;
    std::shared_ptr<tcp_connection> connection;

    // handlers
    std::function<void(std::unique_ptr<websocket>)> on_open_fun;
    std::function<void(std::unique_ptr<websocket>)> on_message_fun;
    std::function<void(std::unique_ptr<websocket>)> on_close_fun;

    // overflow limits
    const size_t receive_buffer_max_size;
    const size_t frames_max_count;

    // output state
    std::vector<asio::const_buffer> payload_buffers;
    std::vector<std::unique_ptr<char[]>> payload_cache;
    size_t payload_length = 0;

    // input state
    // todo: use array sink
    std::vector<char> receive_buffer;
    std::vector<std::unique_ptr<char[]>> frames_cache;
    std::vector<sl::websocket::frame> frames;

public:
    websocket(std::unique_ptr<http_request> req, std::shared_ptr<tcp_connection> conn,
            std::function<void(std::unique_ptr<websocket>)> open_handler,
            std::function<void(std::unique_ptr<websocket>)> message_handler,
            std::function<void(std::unique_ptr<websocket>)> close_handler,
            size_t max_receive_cache_size_bytes = (1024*1024),
            size_t max_cached_frames_count = 1024) :
    request(std::move(req)),
    connection(std::move(conn)),
    on_open_fun(std::move(open_handler)),
    on_message_fun(std::move(message_handler)),
    on_close_fun(std::move(close_handler)),
    receive_buffer_max_size(max_receive_cache_size_bytes),
    frames_max_count(max_cached_frames_count) {
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

    const std::string& get_id() const {
        return request->get_header("Sec-WebSocket-Key");
    }

    tcp_connection& get_connection() {
        return *connection;
    }

    sl::websocket::frame_type message_type() {
        if(frames.size() > 0) {
            return frames[0].type();
        }
        return sl::websocket::frame_type::invalid;
    }

    websocket_detail::msg_data_src message_data() {
        return websocket_detail::msg_data_src(frames);
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
        self->clear_frames_cache();
        process_receive_buffer(std::move(self));
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

    static void broadcast(std::vector<std::shared_ptr<tcp_connection>>& connections,
            sl::io::span<const char> msg,
            sl::websocket::frame_type msg_type = sl::websocket::frame_type::text) {
        // prepare message
        auto holder = std::make_shared<std::vector<char>>();
        auto hbuf = std::array<char, 10>();
        auto header = sl::websocket::frame::make_header(hbuf, msg_type, msg.size());
        holder->resize(header.size() + msg.size());
        std::memcpy(holder->data(), header.data(), header.size());
        std::memcpy(holder->data() + header.size(), msg.data(), msg.size());

        // broadcast message
        for (auto& el : connections) {
            send_broadcast(std::move(el), holder);
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

    void remove_frame_from_receive_buffer(sl::websocket::frame& frame) {
        auto receive_tail = receive_buffer.size() - frame.size();
        if (0 == receive_tail) {
            receive_buffer.clear();
        } else {
            std::memmove(receive_buffer.data(), receive_buffer.data() + frame.size(), receive_tail);
            receive_buffer.resize(receive_tail);
        }
    }

    void add_to_frames_cache(sl::websocket::frame& frame) {
        frames_cache.emplace_back(new char[frame.size()]);
        char* dest = frames_cache.back().get();
        std::memcpy(dest, frame.data(), frame.size());
        frames.emplace_back(sl::websocket::frame({dest, frame.size()}));
        remove_frame_from_receive_buffer(frame);
    }

    void clear_payload() {
        payload_buffers.clear();
        payload_buffers.push_back(asio::buffer(sl::utils::empty_string().data(), 0));
        payload_cache.clear();
        payload_length = 0;
    }

    void clear_frames_cache() {
        // last received frame always lives in cache
        if (frames.size() > 0) {
            remove_frame_from_receive_buffer(frames.back());
        }
        frames_cache.clear();
        frames.clear();
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

    bool receive_buffer_overflow(size_t addition) {
        return receive_buffer.size() + addition > receive_buffer_max_size;
    }

    bool frames_cache_overflow() {
        return frames.size() >= frames_max_count;
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
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket", "Lost context detected in 'post'");
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
                        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "Write error," <<
                                " code: [" << ec << "]" <<
                                " message: [" << ec.message() << "]" <<
                                " id: [" << self->get_id() << "]" <<
                                " path: [" << self->request->get_resource() << "]");
                        on_close(std::move(self), close_status::error);
                    }
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket", "Lost context detected in 'async_write'");
                }
            };
        auto send_handler_stranded = self_ptr->connection->get_strand().wrap(send_handler);
        self_ptr->connection->async_write(self_ptr->payload_buffers, std::move(send_handler_stranded));
    }

    static void send_close(std::unique_ptr<websocket> self, const close_status& status) {
        auto self_ptr = self.get();
        auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
        auto send_handler =
            [self_shared](const asio::error_code&, size_t) {
                auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                if (nullptr != self.get()) {
                    auto self_ptr = self.get();
                    self_ptr->on_close_fun(std::move(self));
                    // self is destroyed on close fun exit
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket", "Lost context detected in 'async_write'");
                }
            };
        auto send_handler_stranded = self_ptr->connection->get_strand().wrap(send_handler);
        auto msg = asio::buffer(std::addressof(status), sizeof(status));
        self_ptr->connection->async_write(std::move(msg), std::move(send_handler_stranded));
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
                        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "Read error," <<
                                " code: [" << ec << "]" <<
                                " message: [" << ec.message() << "]" <<
                                " id: [" << self->get_id() << "]" <<
                                " path: [" << self->request->get_resource() << "]");
                        on_close(std::move(self), close_status::error);
                    }
                } else {
                    STATICLIB_PION_LOG_WARN("staticlib.pion.websocket", "Lost context detected in 'async_read_some'");
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
        // append incoming data to receive buffer
        if (self->receive_buffer_overflow(buf.size())) {
            STATICLIB_PION_LOG_WARN("staticlib.pion.websocket", "Receive buffer overflow" <<
                " id: [" << self->get_id() << "]" <<
                " path: [" << self->request->get_resource() << "]");
            on_close(std::move(self), close_status::overflow);
            return;
        }
        self->add_to_receive_buffer(buf);
        process_receive_buffer(std::move(self));
    }

    static void process_receive_buffer(std::unique_ptr<websocket> self) {
        auto frame = sl::websocket::frame(self->receive_buffer_span());
        // close connection for invalid frame
        if (!frame.is_well_formed()) {
            STATICLIB_PION_LOG_WARN("staticlib.pion.websocket",
                    "Invalid frame received, header: [" << frame.header_hex() << "]" <<
                    " id: [" << self->get_id() << "]" <<
                    " path: [" << self->request->get_resource() << "]");
            on_close(std::move(self), close_status::protocol_error);
            return;
        }
        // read more data for fragmented frame
        if (!frame.is_complete()) {
            receive_internal(std::move(self));
            return;
        }
        // frame is not fragmented on TCP, but may be partial
        if (!frame.is_final()) {
            on_continuation(std::move(self), frame);
            return;
        }
        // complete and final frame received
        process_final_frame(std::move(self), frame);
    }

    static void process_final_frame(std::unique_ptr<websocket> self, sl::websocket::frame frame) {
        self->frames.push_back(frame);
        // choose handler
        switch (frame.type()) {
        case sl::websocket::frame_type::text:
        case sl::websocket::frame_type::binary:
        case sl::websocket::frame_type::continuation:
            on_message(std::move(self));
            break;
        case sl::websocket::frame_type::ping:
            on_ping(std::move(self), frame);
            break;
        case sl::websocket::frame_type::close:
            on_close(std::move(self), close_status::normal);
            break;
        default:
            on_close(std::move(self), close_status::protocol_error);
            break;
        };
    }

    static void on_open(std::unique_ptr<websocket> self) {
        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "WebSocket connection opened," <<
                " id: [" << self->get_id() << "]" <<
                " path: [" << self->request->get_resource() << "]");
        auto self_ptr = self.get();
        self_ptr->on_open_fun(std::move(self));
    }

    static void on_message(std::unique_ptr<websocket> self) {
        auto self_ptr = self.get();
        self_ptr->on_message_fun(std::move(self));
    }

    static void on_close(std::unique_ptr<websocket> self, const close_status& status) {
        STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "Closing WebSocket connection," <<
                " id: [" << self->get_id() << "]" <<
                " path: [" << self->request->get_resource() << "]");
        send_close(std::move(self), status);
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
        if (self->frames_cache_overflow()) {
            STATICLIB_PION_LOG_DEBUG("staticlib.pion.websocket", "Frame fragments overflow" <<
                " id: [" << self->get_id() << "]" <<
                " path: [" << self->request->get_resource() << "]");
            on_close(std::move(self), close_status::overflow);
            return;
        }
        self->add_to_frames_cache(frame);
        // safety precaution for stack overflow
        post_with_strand(std::move(self), websocket::process_receive_buffer);
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
/**
 * Type of function that is used to handle WebSocket connections
 */
using websocket_handler_type = std::function<void(std::unique_ptr<websocket>)>;


} // namespace
}

#endif /* STATICLIB_PION_WEBSOCKET_HPP */

