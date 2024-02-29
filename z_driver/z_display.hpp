#ifndef _Z_DISPLAY_HPP_
#define _Z_DISPLAY_HPP_

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "z_base.hpp"
#include "z_peripheral.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <span>
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
        this->send_polling({{cmd}}, 0);
    }

    void send_cmd_queue(uint8_t cmd)
    {
        this->send_queue({{cmd}}, 0);
    }

    void send_data(std::span<const uint8_t> data)
    {
        this->send_polling(data, 1);
        // if (data.size() != 2)
        // {
        //     // ESP_LOGI(TAG, "size : %d", data.size());
        // }
    }

    void send_data_queue(std::span<const uint8_t> data)
    {
        this->send_queue(data, 1);
    }

  private:
};

constexpr size_t get_buff_size(const size_t wide)
{
    return wide * 10 * 3;
}

template <int DC_Pin, size_t LCD_W = 320, size_t LCD_H = 360>
class z_display_spi_st7796 : public z_display_spi<DC_Pin, get_buff_size(LCD_W)>
{
  public:
    static constexpr size_t _buff_size = get_buff_size(LCD_W);

    z_display_spi_st7796(int mosi_pin, int clk_pin)
        : z_display_spi<DC_Pin, _buff_size>(mosi_pin, clk_pin, 40),
          buff_1(reinterpret_cast<uint8_t *>(_buff_1), _buff_size)
    {
        _buff_1 = heap_caps_malloc(_buff_size, MALLOC_CAP_DMA);

        // soft_reset
        z_sleep_ms(120);
        send_cmd(0x10); // changing mode to Sleep IN.
        z_sleep_ms(120);
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

    void lcd_setraw(uint x_start, uint y_start, uint x_end, uint y_end, std::span<uint8_t> buff)
    {
        //  await_send_finish();
        // LCD_Write area
        ESP_LOGI("tag", "11111");
        if (flag_first_send)
        {
            await_send_finish(6);
        };
        ESP_LOGI("tag", "22222");
        static uint8_t d1 = x_start >> 8;
        static uint8_t d2 = x_start;
        static uint8_t d3 = x_end >> 8;
        static uint8_t d4 = x_end;
        static uint8_t d5 = y_start >> 8;
        static uint8_t d6 = y_start;
        static uint8_t d7 = y_end >> 8;
        static uint8_t d8 = y_end;
        send_cmd_queue(0x2a);
        send_data_queue({{d1, d2, d3, d4}});
        send_cmd_queue(0x2b);
        send_data_queue({{d5, d6, d7, d8}});
        send_cmd_queue(0x2c);
        // send buff
        send_data_queue(buff);
        flag_first_send = true;
    }

    std::span<uint8_t> buff_1;

    size_t disp_w;
    size_t disp_h;

  private:
    //  std::span<const uint8_t, 1024> _buff;
    bool flag_first_send = false;
    void *_buff_1;
    void *_buff_2;

    using _disp = z_display_spi<DC_Pin, _buff_size>;
    using _disp::await_send_finish;
    using _disp::send_cmd;
    using _disp::send_cmd_queue;
    using _disp::send_data;
    using _disp::send_data_queue;
};

// using z_display_spi_st7796s = z_display_spi_st7796<>

#endif