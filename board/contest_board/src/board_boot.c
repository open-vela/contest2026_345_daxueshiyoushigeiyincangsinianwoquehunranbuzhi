/****************************************************************************
 * board/contest_board/src/board_boot.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/kthread.h>
#include <nuttx/sched.h>

#include "board_gpio.h"
#include "board_i2c.h"
#include "board_spiflash.h"
#include "board_audio.h"
#include "board_touch.h"
#include "board_sdmmc.h"
#include "board_c6_wifi.h"
#include "board_display.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_CONTEST2026_345_VELAFIT_KIOSK
extern int velafit_demo_main(int argc, char *argv[]);

/* A kernel thread starts with no file descriptors, so the console has to be
 * wired up before the demo runs.  The demo still prints its acceptance lines
 * to the USB Serial/JTAG console and NSH stays available next to it. */

static int velafit_kiosk_entry(int argc, char *argv[])
{
  int fd = open("/dev/console", O_RDWR);
  int ret;

  if (fd >= 0)
    {
      dup2(fd, 0);
      dup2(fd, 1);
      dup2(fd, 2);
      if (fd > 2)
        {
          close(fd);
        }
    }

  /* The demo must never depend on console transmit.  USB Serial/JTAG only
   * drains while a host is reading, so the serial layer bounds every TX
   * buffer wait; a stalled console can only cost one short wait per write. */

  /* The panel belongs to this thread for the whole power cycle.  A single
   * failed start (sensor not settled, transient DMA fault) must not leave
   * the board frozen on its last frame, so the demo is re-entered until it
   * reports a clean exit on its own duration. */

  for (;;)
    {
      ret = velafit_demo_main(argc, argv);
      if (ret == 0)
        {
          break;
        }

      syslog(LOG_ERR, "ERROR: VelaFit demo aborted: %d, retrying\n", ret);
      sleep(3);
    }

  return ret;
}
#endif

void esp_board_initialize(void)
{
  /* USB Serial/JTAG is initialized by the ESP32-P4 common architecture
   * layer. No board-specific peripheral is required for the first NSH
   * bring-up milestone.
   */
}

int board_app_initialize(uintptr_t arg)
{
  int ret;

#ifdef CONFIG_FS_PROCFS
  ret = nx_mount(NULL, CONFIG_NSH_PROC_MOUNTPOINT, "procfs", 0, NULL);
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_FS_TMPFS
  ret = nx_mount(NULL, CONFIG_LIBC_TMPDIR, "tmpfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount tmpfs at %s: %d\n",
             CONFIG_LIBC_TMPDIR, ret);
    }
#endif

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)
  ret = board_gpio_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_ESPRESSIF_I2C0
  ret = board_i2c_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_ESPRESSIF_SPIFLASH_SMARTFS
  ret = board_spiflash_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#if defined(CONFIG_AUDIO_ES8311) && defined(CONFIG_ESP32P4_I2S0)
  ret = board_audio_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  ret = board_touch_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_ESP32P4_MIPI_DSI
  ret = board_display_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: MIPI DSI display initialization failed: %d\n",
             ret);
    }
#endif

#ifdef CONFIG_CONTEST2026_345_C6_WIFI_AUTOINIT
  ret = board_c6_wifi_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: ESP32-C6 SDIO initialization failed: %d\n",
             ret);
    }
#elif defined(CONFIG_ESP32P4_SDMMC) && \
      !defined(CONFIG_CONTEST2026_345_C6_WIFI)
  ret = board_sdmmc_initialize();
  if (ret < 0)
    {
      /* Non-fatal: SD card may not be inserted at boot */

      ret = 0;
    }
#endif

#ifdef CONFIG_CONTEST2026_345_VELAFIT_KIOSK
  /* The panel comes up on the demo page at power-on, so the board is usable
   * with no console attached and no shell command typed.  kthread_create()
   * prepends the task name as argv[0], so only the arguments follow here. */
  {
    static char * const kiosk_argv[] =
      {
        "demo_now", "3", "600", NULL
      };

    ret = kthread_create("sc2336_probe", SCHED_PRIORITY_DEFAULT, 65536,
                         velafit_kiosk_entry, kiosk_argv);
    if (ret < 0)
      {
        syslog(LOG_ERR, "ERROR: VelaFit kiosk start failed: %d\n", ret);
      }

    ret = 0;
  }
#endif

#ifndef CONFIG_FS_PROCFS
  ret = 0;
#endif

  return ret;
}

#ifdef CONFIG_BOARDCTL_RESET
int board_reset(int status)
{
  up_systemreset();
  return 0;
}
#endif
