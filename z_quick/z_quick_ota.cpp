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

class Session : std::enable_shared_from_this<Session>
{
  public:
    Session(asio::ip::tcp::socket sk) : sk_(std::move(sk))
    {
    }

    void Start()
    {
        auto p{shared_from_this()};
        sk_.async_read_some(asio::buffer(data_, sizeof(data_)), [this, p](asio::error_code ec, size_t len) {
            if (!ec)
            {
                data_[len] = 0;
                std::cout << std::format("recv: {}", data_);
                Start();
            }
        });
    }

  private:
    asio::ip::tcp::socket sk_;
    char data_[1024];
};

class OtaServer
{
  public:
    OtaServer(asio::io_context &io, short port) : acceptor_(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {
        Start();
    }

  private:
    asio::ip::tcp::acceptor acceptor_;

    void Start()
    {
        ESP_LOGI(TAG, "accept");
        acceptor_.async_accept([this](asio::error_code ec, asio::ip::tcp::socket sk) {
            if (!ec)
            {
                std::make_shared<Session>(std::move(sk))->Start();
            }

            Start();
        });
    }
};

void ota_thread()
{
    asio::io_context io;
    OtaServer os(io, 8001);
    io.run();
}

auto start_ota_server() -> void
{
    auto cfg{esp_pthread_get_default_config()};
    cfg.stack_size = 10 * 1024;
    cfg.prio = 0;
    cfg.thread_name = "ota";
    esp_pthread_set_cfg(&cfg);
    std::thread thread_ota(ota_thread);
    thread_ota.detach();
}