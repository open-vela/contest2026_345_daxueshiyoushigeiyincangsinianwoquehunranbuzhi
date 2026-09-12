# ESP32-C6 Wi-Fi link utility

`c6_wifi probe` verifies the on-board ESP32-C6 SDIO identity, bidirectional
CMD53 DMA, and ESP-Hosted initialization event. `c6_wifi version` reads the
coprocessor firmware version. `c6_wifi connect <ssid> <password>` starts STA
mode and waits for the ESP-Hosted connected event.

The password is a runtime argument and must not be committed to configuration
or logs. A successful association verifies the Wi-Fi control plane only; this
stage does not provide a NuttX network interface, DHCP, or IP data traffic.
See `docs/ESP32C6_WIFI_BRINGUP.md` for implementation and acceptance details.
