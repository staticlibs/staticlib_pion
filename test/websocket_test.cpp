
#include <asio.hpp>
#include <iostream>
#include <system_error>
#include <string>
#include <thread>
#include <chrono>

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_response_writer.hpp"
#include "staticlib/pion/http_server.hpp"
#include "staticlib/pion/http_filter_chain.hpp"

#ifdef STATICLIB_PION_USE_LOG4CPLUS
#include <log4cplus/logger.h>
#include <log4cplus/consoleappender.h>
#endif // STATICLIB_PION_USE_LOG4CPLUS

#include "../src/websocket/web_socket/WebSocket.h"

namespace {
    const int buf_size = 4096;
}

class websocket_client
{
	WebSocket ws;

	asio::io_service io_service;
    asio::ip::tcp::endpoint endpoint;
    asio::ip::tcp::socket socket;

    std::error_code ec;

    char msg_buf[buf_size];

    std::thread recieve_thread;

public:
	websocket_client(std::string ip = "127.0.0.1", unsigned short port = 8080): socket(io_service){
        endpoint = asio::ip::tcp::endpoint{asio::ip::address_v4::from_string(ip), port};
    }
    ~websocket_client(){
        if (recieve_thread.joinable()) {
            recieve_thread.join();
        }
    }

    bool connect(){
        try {
            socket.connect(endpoint);
            return true;
        } catch (...) {
        }
        return false;
    }

    void disconnect(){
        socket.cancel();
        socket.close();
        io_service.stop();
    }

	void send_handshake_message(std::string resource){
		string handshake_message{};
		handshake_message += "GET ";
	    handshake_message += resource;
	    handshake_message += " HTTP/1.1\r\n";
	    handshake_message += "Connection: Upgrade\r\n";
	    handshake_message += "Pragma: no-cache\r\n";
	    handshake_message += "Upgrade: websocket\r\n";
	    handshake_message += "Sec-WebSocket-Key: AuRsOCWbejor1Y8Xry7GkQ==\r\n";
	    handshake_message += "\r\n\r\n";
	    socket.send(asio::buffer(handshake_message), 0, ec);
	}

	std::string recieve_message(){
		size_t size = socket.receive(asio::buffer(msg_buf, buf_size), 0, ec);
    	std::string recv_message{msg_buf, size};
    	return recv_message;
	}

	bool do_handshake(std::string resource){
		send_handshake_message(resource);
		std::string answer = recieve_message();

		WebSocketFrameType type = ws.parseHandshake(answer);
        return (WebSocketFrameType::OPENING_FRAME == type);
	}

	void send(std::string input_data){
		std::string message = ws.makeFrame(WebSocketFrameType::TEXT_FRAME, input_data);
		socket.send(asio::buffer(message.c_str(), message.length()), 0, ec);
	}

    void send_close_message(){
        std::string message = ws.makeFrame(WebSocketFrameType::CLOSING_FRAME, "close");
        socket.send(asio::buffer(message.c_str(), message.length()), 0, ec);
    }

    void recieve(){
        socket.async_receive(asio::buffer(msg_buf, buf_size), [this] (const std::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cout << "Error: async_receive" <<std::endl;
                return;
            }
            std::string new_msg;
            WebSocketFrameType type = ws.getFrame(std::string{msg_buf, bytes_transferred}, new_msg);
            // restart recieve
            if ((WebSocketFrameType::CLOSING_FRAME != type) && (WebSocketFrameType::ERROR_FRAME != type)){
                this->recieve();
            } else if (WebSocketFrameType::CLOSING_FRAME == type) {
                this->send_close_message();
            }
        });
    }

    void start_recieve(){
        recieve();
        io_service.run();
    }

    void start_recieve_thread() {
        recieve_thread = std::thread([this] () {
            this->start_recieve();
        });
	}
};

void websocket_test()
{
    websocket_client ws_client;
    ws_client.connect();

    if (!ws_client.do_handshake("/hello")) {
        std::cout << "error handshake" << std::endl;
    } else {
        ws_client.start_recieve_thread();
        ws_client.send("hello");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ws_client.send_close_message();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ws_client.disconnect();
}


const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8080;

#ifdef STATICLIB_PION_USE_LOG4CPLUS
const std::string CONSOLE_APPENDER_LAYOUT = "%d{%H:%M:%S} [%-5p %-15.15c] %m%n";

log4cplus::SharedAppenderPtr create_console_appender() {
    log4cplus::SharedAppenderPtr res{new log4cplus::ConsoleAppender()};
    res->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(CONSOLE_APPENDER_LAYOUT)));
    return res;
}
#endif // STATICLIB_PION_USE_LOG4CPLUS


std::vector<sl::pion::websocket_service_ptr> ws_services;

sl::pion::websocket_service_data generate_ws_data(std::string resource){
    sl::pion::websocket_service_data ws_data;

    ws_data.open_handler = [] (void* data, void* user_data) {
        (void) data;
        (void) user_data;
        STATICLIB_PION_LOG_INFO("open_handler", "worked!");
    };

    ws_data.close_handler = [] (void* data, void* user_data) {
        (void) data;
        (void) user_data;
        STATICLIB_PION_LOG_INFO("close_handler", "worked!");
    };

    ws_data.error_handler = [] (void* data, void* user_data, const char *str, int len) {
        (void) data;
        (void) user_data;
        std::string message{str, static_cast<size_t>(len)};
        STATICLIB_PION_LOG_INFO("error_handler", "Error recieved [" + message + "]");
    };

    ws_data.message_handler = [] (void* data, void* user_data, const char *str, int len) {
        (void) data;
        (void) user_data;
        std::string message{str, static_cast<size_t>(len)};
        STATICLIB_PION_LOG_INFO("message_handler", "Message recieved [" + message + "]");
    };

    ws_data.websocket_prepare_handler = [] (sl::pion::websocket_service_ptr service) {
        STATICLIB_PION_LOG_INFO("prepare_handler", "worked!");
        ws_services.push_back(service); // store to prevent delete by shared_ptr mechanism
    };

    ws_data.resource = resource;

    return ws_data;
}


void test_pion() {
#ifdef STATICLIB_PION_USE_LOG4CPLUS
    #ifdef STATICLIB_PION_USE_LOG4CPLUS_STATIC
    // need initialization with static log4cplus
    log4cplus::initialize();
    #endif // STATICLIB_PION_USE_LOG4CPLUS_STATIC
    auto fa = create_console_appender();
    log4cplus::Logger::getRoot().addAppender(fa);
    log4cplus::Logger::getRoot().setLogLevel(log4cplus::ALL_LOG_LEVEL);
    log4cplus::Logger::getInstance("pion").setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else // std out logging
    STATICLIB_PION_LOG_SETLEVEL_INFO(STATICLIB_PION_GET_LOGGER("staticlib.pion"))
#endif // STATICLIB_PION_USE_LOG4CPLUS
    // pion
    sl::pion::http_server server(2, TCP_PORT);

    sl::pion::websocket_service_data ws_data = generate_ws_data("/hello");
    server.add_websocket_handler("hello", ws_data);

    server.get_scheduler().set_thread_stop_hook([](const std::thread::id& tid) STATICLIB_NOEXCEPT {
        std::cout << "Thread stopped: " << tid << std::endl;
    });
    server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    websocket_test();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    server.stop(true);
}

int main() {
    try {
        test_pion();
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}


