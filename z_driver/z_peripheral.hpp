#ifndef _Z_PERIPHERAL_HPP_
#define _Z_PERIPHERAL_HPP_

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "z_base.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <sys/stat.h>

class z_gpio
{
  public:
    z_gpio(const int num) : _num(static_cast<gpio_num_t>(num))
    {
    }

    void set()
    {
        gpio_set_level(_num, 1);
    }

    void clr()
    {
        gpio_set_level(_num, 0);
    }

    constexpr int get_io_num()
    {
        return _num;
    };

  private:
    const gpio_num_t _num;
};

class z_gpio_ppout : public z_gpio
{
  public:
    z_gpio_ppout(const int num) : z_gpio(num)
    {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = 1ULL << num;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        // io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        //  io_conf .intr_type = NULL;
        gpio_config(&io_conf);
    }

  private:
};

class z_gpio_odout : public z_gpio
{
};

// spi device

template <int DC_Pin> static void spi_pre_transfer_callback(spi_transaction_t *t)
{
    auto dc = (int)t->user;
    gpio_set_level(static_cast<gpio_num_t>(DC_Pin), dc);
}

template <int DC_Pin, size_t Trans_Size> class z_spi_master
{
  public:
    // 无miso
    z_spi_master(int mosi_pin, int clk_pin, int spi_mhz) : dc_pin(DC_Pin)
    {
        spi_bus_config_t buscfg{};
        buscfg.mosi_io_num = mosi_pin;
        buscfg.miso_io_num = -1;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = Trans_Size + 40;
        buscfg.sclk_io_num = clk_pin;

        spi_device_interface_config_t devcfg{};
        devcfg.mode = 0;
        devcfg.clock_speed_hz = spi_mhz * 1000 * 1000;
        devcfg.spics_io_num = -1;
        devcfg.queue_size = 7;
        devcfg.pre_cb = spi_pre_transfer_callback<DC_Pin>;

        auto ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        ESP_ERROR_CHECK(ret);
        ret = spi_bus_add_device(SPI2_HOST, &devcfg, &_spi);
        ESP_ERROR_CHECK(ret);
    }

    // z_spi_master(gpio_num_t dc_pin)
    // {
    // }

    z_gpio_ppout dc_pin;

    void send(std::span<const uint8_t> data, int user)
    {
        spi_transaction_t t{};
        t.length = data.size() * 8;
        t.tx_buffer = data.data();
        t.user = (void *)user;
        ESP_ERROR_CHECK(spi_device_transmit(_spi, &t));
    }

    void send_polling(std::span<const uint8_t> data, int user)
    {
        spi_transaction_t t{};
        t.length = data.size() * 8;
        t.tx_buffer = data.data();
        t.user = (void *)user;
        ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, &t));
    }

    void send_queue(std::span<const uint8_t> data, int user)
    {
        spi_transaction_t t{};
        t.length = data.size() * 8;
        t.tx_buffer = data.data();
        t.user = (void *)user;
        ESP_ERROR_CHECK(spi_device_queue_trans(_spi, &t, portMAX_DELAY));
    }

    void await_send_finish(size_t times)
    {
        spi_transaction_t *rtrans;
        esp_err_t ret;
        for (int x = 0; x < times; x++)
        {
            ret = spi_device_get_trans_result(_spi, &rtrans, portMAX_DELAY);
            assert(ret == ESP_OK);
        }
    }

  private:
    spi_device_handle_t _spi;
};

#endif