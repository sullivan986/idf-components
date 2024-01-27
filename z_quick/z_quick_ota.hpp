#include "z_quick_ota.h"
#include "asio/buffer.hpp"
#include "asio/error_code.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include <chrono>
#include <cstddef>
#include <esp_log.h>
#include <esp_pthread.h>
#include <format>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/_intsup.h>
#include <sys/types.h>
#include <thread>

static const char *TAG = "Z_QUICK_OTA";

using asio::ip::tcp;

class session : public std::enable_shared_from_this<session>
{
  public:
    session(tcp::socket socket) : socket_(std::move(socket))
    {
    }

    void start()
    {
        auto self{shared_from_this()};
        socket_.async_read_some(asio::buffer(data_, max_length), [this, self](std::error_code ec, std::size_t length) {
            if (!ec)
            {
                data_[length] = 0;
                std::cout << data_ << std::endl;
                start();
            }
            else
            {
                ESP_LOGW(TAG, "socket disconnect");
            }
        });
    }

  private:
    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
};

class server
{
  public:
    server(asio::io_context &io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

  private:
    tcp::acceptor acceptor_;
    void do_accept()
    {
        acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
            if (!ec)
            {
                std::make_shared<session>(std::move(socket))->start();
            }
            do_accept();
        });
    }
};

void ota_thread()
{
    asio::io_context io;
    server os(io, 8001);
    io.run();
}

auto start_ota_server() -> void
{
    auto cfg{esp_pthread_get_default_config()};
    cfg.stack_size = 12 * 1024;
    cfg.prio = 1;
    cfg.thread_name = "ota";
    esp_pthread_set_cfg(&cfg);
    std::thread thread_ota(ota_thread);
    thread_ota.detach();
}