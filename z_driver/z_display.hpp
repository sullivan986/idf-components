#ifndef _Z_DISPLAY_HPP_
#define _Z_DISPLAY_HPP_

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "z_base.hpp"
#include "z_peripheral.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <span>
#include <sys/types.h>
#include <thread>
#include <vector>

template <int DC_Pin, size_t Trans_Size> class z_display_spi : public z_spi_master<DC_Pin, Trans_Size>
{
  public:
    z_display_spi(int mosi_pin, int clk_pin, int spi_mhz) : z_spi_master<DC_Pin, Trans_Size>(mosi_pin, clk_pin, spi_mhz)
    {
    }

    void send_cmd(uint8_t cmd)
    {
        this->send({{cmd}}, 0);
    }

    void send_cmd_polling(uint8_t cmd)
    {
        this->send_polling({{cmd}}, 0);
    }

    void send_cmd_queue(uint8_t cmd)
    {
        this->send_queue({{cmd}}, 0);
    }

    void send_data(std::span<const uint8_t> data)
    {
        this->send(data, 1);
    }

    void send_data_polling(std::span<const uint8_t> data)
    {
        this->send_polling(data, 1);
    }

    void send_data_queue(std::span<const uint8_t> data)
    {
        this->send_queue(data, 1);
    }

  private:
};

constexpr size_t getbuff_size(const size_t wide)
{
    return wide * 20 * 3;
}

template <int DC_Pin, size_t LCD_W = 320, size_t LCD_H = 360>
class z_display_spi_st7796 : public z_display_spi<DC_Pin, getbuff_size(LCD_W)>
{
  public:
    static constexpr size_t buff_size = getbuff_size(LCD_W);

    auto get_buff_span()
    {
        return std::span<uint8_t>(reinterpret_cast<uint8_t *>(_buff_1), buff_size);
    }

    z_display_spi_st7796(int mosi_pin, int clk_pin) : z_display_spi<DC_Pin, buff_size>(mosi_pin, clk_pin, 40)
    {
        // ESP_LOGI("tag", "%d, %d", buff_size, buff_1.size());
        _buff_1 = heap_caps_malloc(buff_size, MALLOC_CAP_DMA);

        // soft_reset
        z_sleep_ms(120);
        send_cmd(0x10); // changing mode to Sleep IN.
        z_sleep_ms(240);
        send_cmd(0x01); // soft_reset
        z_sleep_ms(120);

        // Sleep out
        send_cmd(0x11);
        z_sleep_ms(50);
        // start setting
        send_cmd(0Xf0);
        send_data({{0xc3}});
        send_cmd(0Xf0);
        send_data({{0x96}});
        // Memory Access Control
        send_cmd(0x36);
        send_data({{0x48}});
        // 7 : 24bit(8+8+8) , 5 : 16bit(5+6+5)
        send_cmd(0x3a);
        send_data({{0x07}});
        // Inversion
        // send_cmd(0xb4);
        // send_data({{0x01}});
        // ????
        // send_cmd(0xb6);
        // send_data({{0x80, 0x02, 0x3b}});
        // 周期时间
        send_cmd(0xe8);
        send_data({{
            0x40,
            0x8a,
            0x00,
            0x00,
            0x29, // 震源均衡周期时间
            0x19, // 震源均衡周期时间
            0xa5,
            0x33,
        }});
        // power
        send_cmd(0Xc5);
        send_data({{0x27}});
        // power
        send_cmd(0Xc2);
        send_data({{0xa7}});
        // color GAMA adjust
        send_cmd(0Xe0);
        send_data({{
            0xf0,
            0x01,
            0x06,
            0x0f,
            0x12,
            0x1d,
            0x36,
            0x54,
            0x44,
            0x0c,
            0x18,
            0x16,
            0x13,
            0x15,
        }});
        send_cmd(0Xe1);
        send_data({{
            0xf0,
            0x01,
            0x05,
            0x0a,
            0x0b,
            0x07,
            0x32,
            0x44,
            0x44,
            0x0c,
            0x18,
            0x17,
            0x13,
            0x16,
        }});
        // end setting
        send_cmd(0Xf0);
        send_data({{0x3c}});
        // end setting
        send_cmd(0Xf0);
        send_data({{0x69}});
        // Inversion color
        send_cmd(0X21);
        // Display on
        send_cmd(0X29);
    }

    void lcd_setraw(size_t x_start, size_t y_start, size_t x_end, size_t y_end, std::span<uint8_t> data)
    {
        //  await_send_finish();
        // LCD_Write area
        // auto size = (x_end - x_start + 1) * (y_end - y_start + 1);
        // if (size > buff_size)
        // {
        //     ESP_LOGI("z_display", "buffer error");
        //     return;
        // }
        // std::span<uint8_t> buff_1(reinterpret_cast<uint8_t *>(_buff_1), size);

        static uint8_t d1 = x_start >> 8;
        static uint8_t d2 = x_start;
        static uint8_t d3 = x_end >> 8;
        static uint8_t d4 = x_end;
        static uint8_t d5 = y_start >> 8;
        static uint8_t d6 = y_start;
        static uint8_t d7 = y_end >> 8;
        static uint8_t d8 = y_end;
        send_cmd(0x2a);
        send_data({{d1, d2, d3, d4}});
        send_cmd(0x2b);
        send_data({{d5, d6, d7, d8}});
        send_cmd(0x2c);
        // send buff
        send_data(data);
    }

  private:
    void *_buff_1;
    void *_buff_2;

    using _disp = z_display_spi<DC_Pin, buff_size>;
    using _disp::await_send_finish;
    using _disp::send_cmd;
    using _disp::send_cmd_queue;
    using _disp::send_data;
    using _disp::send_data_queue;
};

// using z_display_spi_st7796s = z_display_spi_st7796<>

#endif