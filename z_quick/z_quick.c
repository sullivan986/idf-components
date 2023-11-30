#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
// #include "lwip/lwip_napt.h"

#include "z_quick.h"

// APP tag
static const char *TAG = "Z_QUICK";
static const char *TAG_STA = "Z_QUICK_STA";
static const char *TAG_AP = "Z_QUICK_AP";

static EventGroupHandle_t s_wifi_event_group;

// ----------------------------------------------------------
// currency quick
// blink
static void z_s_blink(void *pv)
{
    gpio_num_t num_gpio = *(gpio_num_t *)pv;
    gpio_reset_pin(num_gpio);
    gpio_set_direction(num_gpio, GPIO_MODE_OUTPUT);
    while (true)
    {
        gpio_set_level(num_gpio, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(num_gpio, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void z_cur_blink(gpio_num_t num_gpio)
{
    xTaskCreate(z_s_blink, "z_blink", 2800, (void *)&num_gpio, 11, NULL);
}

// breathing led
static void z_s_ledc(void *pv)
{
    int num_gpio = *(int *)pv;
    ledc_timer_config_t pwmled_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&pwmled_timer);

    ledc_channel_config_t pwmled_channel = {
        .gpio_num = num_gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        // .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 0,
    };
    ledc_channel_config(&pwmled_channel);

    ledc_fade_func_install(0);

    while (true)
    {
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4096, 3000);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 3000);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}
void z_cur_ledc(gpio_num_t num_gpio)
{
    xTaskCreate(z_s_ledc, "z_ledc", 1200, (void *)&num_gpio, 11, NULL);
}
// check memfree
static void z_s_loglist(void *pv)
{
    char *pbuffer = malloc(2048);
    while (true)
    {
        printf("--------------- heap:%lu kb ---------------------\r\n", esp_get_free_heap_size() / 1024);
        vTaskList(pbuffer);
        printf("%s", pbuffer);
        printf("------------------------------------------------\r\n");
        free(pbuffer);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}
void z_cur_loglist()
{
    xTaskCreate(z_s_loglist, "z_loglist", 2400, NULL, 11, NULL);
}

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
            ESP_LOGI(TAG, "UB! check the code");
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

static void z_quick_init()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // creat the event group
    // s_wifi_event_group = xEventGroupCreate();

    // netif init
    esp_netif_init();

    // create default event loop
    esp_event_loop_create_default();

    // create freertos event group
    s_wifi_event_group = xEventGroupCreate();
}

void z_quick_connect_to(char *ssid, char *passwd)
{
    z_quick_init();
    ESP_LOGI(TAG, "link to wifi: ssid:%s passwd:%s", ssid, passwd);
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
    wifi_config_t wifi_config_sta = {
        .sta.threshold.authmode = WIFI_AUTH_WPA2_PSK,
    };
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

void _z_quick_open_ap(const char *ssid, const char *passwd, uint8_t len)
{
    z_quick_init();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
    // IP_EVENT_AP_STAIPASSIGNED, &ap_event_handler, NULL, NULL));
    wifi_config_t wifi_config_ap = {
        .ap =
            {
                .max_connection = WIFI_AP_MAX_CON,
                .ssid_len = len,
                .pmf_cfg.required = false,
            },
    };

    strcpy((char *)wifi_config_ap.ap.ssid, ssid);

    if (passwd == NULL)
    {
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
        strcpy((char *)wifi_config_ap.ap.password, "");
    }
    else
    {
        wifi_config_ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        strcpy((char *)wifi_config_ap.ap.password, passwd);
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap);
    esp_wifi_start();
    ESP_LOGI(TAG, "ap init finished");
}

void z_quick_sta_and_ap(const char *sta_ssid, const char *sta_passwd)
{
    z_quick_init();
    ESP_LOGI(TAG, "link to wifi: ssid:%s passwd:%s", sta_ssid, sta_passwd);
    ESP_LOGI(TAG, "open a ap: ssid:espcore passwd:none");
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
    // esp32 sta config
    wifi_config_t wifi_config_sta = {
        .sta.threshold.authmode = WIFI_AUTH_WPA2_PSK,
    };
    strcpy((char *)wifi_config_sta.sta.ssid, sta_ssid);
    strcpy((char *)wifi_config_sta.sta.password, sta_passwd);
    // esp32 ap config
    wifi_config_t wifi_config_ap = {
        .ap =
            {
                .ssid = WIFI_AP_NAME,
                .ssid_len = WIFI_AP_NAME_LEN,
                .password = "",
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = WIFI_AP_MAX_CON,
            },
    };
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap);
    esp_wifi_start();
    // 等待链接成功
    xEventGroupWaitBits(s_wifi_event_group, WIFI_SUCC_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(s_wifi_event_group);
    ESP_LOGI(TAG, "staap init finished");

    // enable NAT
    // ip_napt_enable(ESP_IP4TOADDR(192, 168, 4, 1), 1);
}