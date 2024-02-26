idf_component_register(
        SRCS "z_quick_wifi.hpp" "z_quick_ota.hpp" "z_quick_algorithm.hpp" "z_quick_ble.hpp"
        INCLUDE_DIRS .
        PRIV_REQUIRES  z_driver unity nvs_flash esp_wifi esp_event esp_netif asio pthread bt console driver
)