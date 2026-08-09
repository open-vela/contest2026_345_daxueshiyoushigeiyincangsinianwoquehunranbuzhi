/****************************************************************************
 * Contest 2026 team 345 ESP32-P4X Function EV Board
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>

void esp_board_initialize(void)
{
  /* USB Serial/JTAG is initialized by the ESP32-P4 common architecture
   * layer. No board-specific peripheral is required for the first NSH
   * bring-up milestone.
   */
}

int board_app_initialize(uintptr_t arg)
{
#ifdef CONFIG_FS_PROCFS
  return nx_mount(NULL, CONFIG_NSH_PROC_MOUNTPOINT, "procfs", 0, NULL);
#else
  return 0;
#endif
}

#ifdef CONFIG_BOARDCTL_RESET
int board_reset(int status)
{
  up_systemreset();
  return 0;
}
#endif
