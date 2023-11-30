idf_component_register(
        SRCS z_quick_wifi.cpp
        INCLUDE_DIRS include
        PRIV_REQUIRES driver unity nvs_flash esp_wifi esp_event esp_netif asio
)