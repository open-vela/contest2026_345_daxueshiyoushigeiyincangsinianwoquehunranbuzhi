/****************************************************************************
 * app/c6_wifi/c6_wifi_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>

#include <arch/board/board.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  int ret;

  if ((argc != 2 ||
       (strcmp(argv[1], "probe") != 0 &&
        strcmp(argv[1], "version") != 0)) &&
      (argc != 4 || strcmp(argv[1], "connect") != 0))
    {
      fprintf(stderr,
              "Usage: c6_wifi {probe|version|connect <ssid> "
              "<password>}\n");
      return 1;
    }

  ret = board_c6_wifi_initialize();
  if (ret < 0)
    {
      fprintf(stderr, "ESP32-C6 SDIO probe failed: %d\n", ret);
      return 1;
    }

  if (strcmp(argv[1], "probe") == 0)
    {
      printf("ESP32-C6 SDIO probe passed\n");
      return 0;
    }

  if (strcmp(argv[1], "connect") == 0)
    {
      ret = board_c6_wifi_connect(argv[2], argv[3]);
      if (ret < 0)
        {
          fprintf(stderr, "ESP32-C6 Wi-Fi connect failed: %d\n", ret);
          return 1;
        }

      printf("ESP32-C6 Wi-Fi connected: SSID=%s\n", argv[2]);
      return 0;
    }

  ret = board_c6_wifi_version(&major, &minor, &patch);
  if (ret < 0)
    {
      fprintf(stderr, "ESP32-C6 Hosted RPC failed: %d\n", ret);
      return 1;
    }

  printf("ESP32-C6 Hosted firmware: %lu.%lu.%lu\n",
         (unsigned long)major, (unsigned long)minor,
         (unsigned long)patch);
  return 0;
}
