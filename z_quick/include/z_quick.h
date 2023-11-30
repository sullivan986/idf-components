#ifndef Z_QUICK_H
#define Z_QUICK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "string.h"

// event 宏
#define WIFI_AP_NAME "esplink"
#define WIFI_AP_NAME_LEN 7
#define WIFI_AP_PASSWD "12345678"
#define WIFI_AP_MAX_CON 2

#define WIFI_SUCC_BIT BIT0
#define WIFI_FAIL_BIT BIT1

    // 通用
    // 闪烁灯
    void z_cur_blink(gpio_num_t num_gpio);

    // 呼吸灯
    void z_cur_ledc(gpio_num_t num_gpio);

    // task 检查
    void z_cur_loglist();

    // wifi 相关
    // 初始化通用参数

    // 快速连接到一个 wifi
    void z_quick_connect_to(char *ssid, char *passwd);

    // 开启热点

    void _z_quick_open_ap(const char *ssid, const char *passwd, uint8_t len);
    inline void z_quick_open_ap(const char *ssid, const char *passwd)
    {
        _z_quick_open_ap(ssid, passwd, strlen(ssid));
    }

    // 快速开启 STA 与 AP 共存模式
    void z_quick_sta_and_ap(const char *sta_ssid, const char *sta_passwd);

#ifdef __cplusplus
}
#endif

#endif