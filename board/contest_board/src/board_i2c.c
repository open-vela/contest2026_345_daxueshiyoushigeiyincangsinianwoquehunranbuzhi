/****************************************************************************
 * board/contest_board/src/board_i2c.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_ESPRESSIF_I2C0

#include <errno.h>

#include <debug.h>
#include <nuttx/i2c/i2c_master.h>

#include "espressif/esp_i2c.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int board_i2c_initialize(void)
{
  struct i2c_master_s *i2c;
  int ret;

  i2c = esp_i2cbus_initialize(ESPRESSIF_I2C0);
  if (i2c == NULL)
    {
      i2cerr("Failed to initialize ESP32-P4 I2C0\n");
      return -ENODEV;
    }

  ret = i2c_register(i2c, 0);
  if (ret < 0)
    {
      i2cerr("Failed to register /dev/i2c0: %d\n", ret);
      esp_i2cbus_uninitialize(i2c);
    }

  return ret;
}

#endif /* CONFIG_ESPRESSIF_I2C0 */
