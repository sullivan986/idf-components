#include "z_quick_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#define WIFI_SUCC_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "Z_QUICK";
static const char *TAG_STA = "Z_QUICK_STA";
static const char *TAG_AP = "Z_QUICK_AP";

static EventGroupHandle_t s_wifi_event_group;

// ----------------------------------------------------------
// wifi quick
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "event_base:%s, event_id:%ld", event_base, event_id);

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START: // STA模式启动
            ESP_LOGI(TAG_STA, "STA connect start");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_STOP: // STA模式关闭
            ESP_LOGI(TAG_STA, "STA connect stop");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG_STA, "wifi connect successful!");
            break;

        case WIFI_EVENT_STA_DISCONNECTED: // STA模式断开连接
            esp_wifi_connect();
            ESP_LOGI(TAG_STA, "re-connecting...");
            break;

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG_AP, "AP start");
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG_AP, "AP stop");
            break;

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG_AP, "a device connected: mac:" MACSTR ", aid:%d", MAC2STR(event->mac), event->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG_AP, "device disconnect: mac:" MACSTR ", aid:%d", MAC2STR(event->mac), event->aid);
            break;
        }
        default:
            ESP_LOGI(TAG, "UB! check the code:%ld", event_id);
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP: { // esp32从路由器获取到ip
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG_STA, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(s_wifi_event_group, WIFI_SUCC_BIT);
            break;
        }
        default:
            break;
        }
    }
}

z_wifi_controller::z_wifi_controller()
{
    esp_netif_init();
    esp_event_loop_create_default();
    s_wifi_event_group = xEventGroupCreate();
}

auto z_wifi_controller::connect_to(char *ssid, char *passwd) -> void
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
    wifi_config_t wifi_config_sta{.sta{.threshold{.authmode{WIFI_AUTH_WPA2_PSK}}}};
    strcpy((char *)wifi_config_sta.sta.ssid, ssid);
    strcpy((char *)wifi_config_sta.sta.password, passwd);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta);
    esp_wifi_start();
    // 等待链接成功
    xEventGroupWaitBits(s_wifi_event_group, WIFI_SUCC_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(s_wifi_event_group);
    ESP_LOGI(TAG, "sta init finished");
}