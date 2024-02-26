idf_component_register(
        SRCS "z_display.hpp" "z_peripheral.hpp" "z_base.hpp"
        INCLUDE_DIRS .
        PRIV_REQUIRES driver unity nvs_flash pthread 
)