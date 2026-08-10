/****************************************************************************
 * Contest 2026 team 345 ESP32-P4X Function EV Board
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>

#include "board_gpio.h"

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

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)
  ret = board_gpio_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_FS_PROCFS
  ret = nx_mount(NULL, CONFIG_NSH_PROC_MOUNTPOINT, "procfs", 0, NULL);
  if (ret < 0)
    {
      return ret;
    }
#else
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
