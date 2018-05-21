#ifndef WEBSOCKET_WORKER_H
#define WEBSOCKET_WORKER_H

#include "websocket_handler.hpp"

namespace staticlib {
namespace pion {

using basic_handler_type = std::function<void(void*, void*)>; // only data
typedef basic_handler_type onopen_handler;
typedef basic_handler_type onclose_handler;

using basic_handler_message_type = std::function<void(void*, void*, const char *, int )>; // data, message
typedef basic_handler_message_type onmessage_handler;
typedef basic_handler_message_type onerror_handler;

class websocket_service;
struct websocket_service_data;

using websocket_service_data_ptr = std::shared_ptr<websocket_service_data>;
using websocket_service_ptr = std::shared_ptr<websocket_service>;

using websocket_prepare_handler_type = std::function<void(websocket_service_ptr)>;

struct websocket_service_data {
    onmessage_handler message_handler;
    onopen_handler    open_handler;
    onclose_handler   close_handler;
    onerror_handler   error_handler;

    websocket_prepare_handler_type websocket_prepare_handler;

    void* message_data;
    void* open_data;
    void* close_data;
    void* error_data;

    std::string resource;
    void* user_data;

    websocket_service_data(const websocket_service_data& data){ // copy constructor
        this->message_handler = data.message_handler;
        this->open_handler    = data.open_handler;
        this->close_handler   = data.close_handler;
        this->error_handler   = data.error_handler;
        this->message_data    = data.message_data;
        this->open_data       = data.open_data;
        this->close_data      = data.close_data;
        this->error_data      = data.error_data;
        this->resource        = data.resource;
        this->user_data       = data.user_data;
        this->websocket_prepare_handler = data.websocket_prepare_handler;
    }

    websocket_service_data& operator =(const websocket_service_data& data){
        this->message_handler = data.message_handler;
        this->open_handler    = data.open_handler;
        this->close_handler   = data.close_handler;
        this->error_handler   = data.error_handler;
        this->message_data    = data.message_data;
        this->open_data       = data.open_data;
        this->close_data      = data.close_data;
        this->error_data      = data.error_data;
        this->resource        = data.resource;
        this->user_data       = data.user_data;
        this->websocket_prepare_handler = data.websocket_prepare_handler;
        return *this;
    }

    websocket_service_data(){
        open_data = nullptr;
        close_data = nullptr;
        error_data = nullptr;
        user_data = nullptr;
        message_data = nullptr;
    }
    // Деструктор не нужен все это будет убиваться когда умрет сервер, который это запустил
};

class websocket_service {
    websocket_handler ws_handler;
    // Задаем обработчики для событий
    websocket_service_data ws_data;
    std::thread recieve_thread;
public:
    // Создаем воркер который будет обрабатывать запросы через websocket так как это
    // Сейчас реализовано в wilton_websocket_server
    // Нужно задать для него хэндлеры и данные

    websocket_service(staticlib::pion::tcp_connection_ptr tcp_conn, websocket_service_data ws_data);
    ~websocket_service();

    void start_with_message(std::string message, websocket_service_ptr service);
    void setup_data(const websocket_service_data& input_data);
    void setup_user_data(void* data);
    void* get_user_data();

    void stop();
    void send(std::string message);

    void run_open_handler();
    void run_error_handler(const string& error_message);
    void run_close_handler();
    void run_message_handler(const string& out_message);

    void send_pong_frame(const string ping_message);
};

} //namespace pion
} //namespace staticlib


#endif /* WEBSOCKET_WORKER_H */

