# Contest 2026 team 345 ESP32-C6 Wi-Fi link utility

if(CONFIG_LVX_USE_DEMO_CONTEST2026_345_C6_WIFI)
  nuttx_add_application(
    NAME c6_wifi
    SRCS c6_wifi_main.c
    STACKSIZE 4096)
endif()
