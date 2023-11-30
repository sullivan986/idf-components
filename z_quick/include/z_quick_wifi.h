#ifndef Z_QUICK_WIFI_H
#define Z_QUICK_WIFI_H

#include "string_view"

class z_wifi_controller
{
  public:
    z_wifi_controller();
    z_wifi_controller(z_wifi_controller &&) = default;
    z_wifi_controller(const z_wifi_controller &) = default;
    z_wifi_controller &operator=(z_wifi_controller &&) = default;
    z_wifi_controller &operator=(const z_wifi_controller &) = default;
    ~z_wifi_controller();

    auto connect_to(char *ssid, char *passwd) -> void;

  private:
};

#endif