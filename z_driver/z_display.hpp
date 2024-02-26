#ifndef _Z_DISPLAY_HPP_
#define _Z_DISPLAY_HPP_

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "z_base.hpp"
#include "z_peripheral.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <thread>

template <int DC_Pin> class z_display_spi : public z_spi_master<DC_Pin>
{
  public:
    z_display_spi(int mosi_pin, int clk_pin, int rst_pin, int spi_mhz)
        : z_spi_master<DC_Pin>(mosi_pin, clk_pin, spi_mhz), _rst_pin(rst_pin)
    {
        reset();
        soft_reset();
    }

    void soft_reset()
    {
        send_cmd(0x01);
    }

    void reset()
    {
        _rst_pin.n();
        z_sleep_ms(100);
        _rst_pin.set();
        z_sleep_ms(100);
    }

    void send_cmd(uint8_t cmd)
    {
        this->send_polling({{cmd}}, 0);
    }

    void send_data_short(std::span<const uint8_t> data)
    {
        this->send_polling(data, 1);
    }

    void send_data_long(std::span<const uint8_t> data)
    {
        this->send_polling(data, 1);
    }

  private:
    z_gpio_ppout _rst_pin;
    // static constexpr auto p = dc_pin::real_addr;
};

template <int DC_Pin> class z_display_spi_st7796 : public z_display_spi<DC_Pin>
{
  public:
    z_display_spi_st7796(int mosi_pin, int clk_pin, int rst_pin) : z_display_spi<DC_Pin>(mosi_pin, clk_pin, rst_pin, 8)
    {
        // Sleep out
        send_cmd(0x11);
        z_sleep_ms(50);
        // 激活所有指令
        send_cmd(0Xf0);
        send_data_short({{0xc3}});
        send_cmd(0Xf0);
        send_data_short({{0x96}});
        // Memory Access Control
        send_cmd(0x36);
        send_data_short({{0x48}});
        // send 24bit(8+8+8)
        send_cmd(0x3a);
        send_data_short({{0x07}});
        // Inversion
        // send_cmd(0xb4);
        // send_data_short({{0x01}});
        // ????
        // send_cmd(0xb6);
        // send_data_short({{0x80, 0x02, 0x3b}});
        // 周期时间
        send_cmd(0xe8);
        send_data_short({{
            0x40,
            0x8a,
            0x00,
            0x00,
            0x29, // 震源均衡周期时间
            0x19, // 震源均衡周期时间
            0xa5,
            0x33,
        }});
        //
        send_cmd(0Xc5);
        send_data_short({{0x27}});
        //
        send_cmd(0Xc2);
        send_data_short({{0xa7}});
        // color GAMA adjust
        send_cmd(0Xe0);
        send_data_short({{
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
        send_data_short({{
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
        //
        send_cmd(0Xf0);
        send_data_short({{0x3c}});
        //
        send_cmd(0Xf0);
        send_data_short({{0x69}});
    }

    void LCD_SetPos(unsigned int Xstart, unsigned int Ystart, unsigned int Xend, unsigned int Yend)
    {
        uint8_t x1 = Xstart >> 8;
        uint8_t x2 = Xstart;
        uint8_t x3 = Xend >> 8;
        uint8_t x4 = Xend;
        uint8_t y1 = Ystart >> 8;
        uint8_t y2 = Ystart;
        uint8_t y3 = Yend >> 8;
        uint8_t y4 = Yend;

        send_cmd(0x2a);
        send_data_short({{x1, x2, x3, x4}});
        send_cmd(0x2b);
        send_data_short({{y1, y2, y3, y4}});
        send_cmd(0x2c); // LCD_WriteCMD(GRAMWR);
    }
#define LCD_W 320
#define LCD_H 360
    void ClearScreen(uint8_t R, uint8_t G, uint8_t B)
    {
        printf("ClearScreen");
        unsigned int i, j;
        LCD_SetPos(0, 0, LCD_W - 1, LCD_H - 1);
        for (i = 0; i < LCD_H; i++)
        {
            for (j = 0; j < LCD_W; j++)
                send_data_short({{R, G, B}});
        }
    }

  private:
    //  std::span<const uint8_t, 1024> _buff;

    using z_display_spi<DC_Pin>::send_cmd;
    using z_display_spi<DC_Pin>::send_data_short;
};

// using z_display_spi_st7796s = z_display_spi_st7796<>

#endif