#ifndef Z_QUICK_WIFI_H
#define Z_QUICK_WIFI_H

class z_wifi_controller
{
  public:
    z_wifi_controller();

    auto connect_to(const char *ssid, const char *passwd) -> void;

  private:
};

#endif