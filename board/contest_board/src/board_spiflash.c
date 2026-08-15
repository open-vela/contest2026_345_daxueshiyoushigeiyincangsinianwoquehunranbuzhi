/****************************************************************************
 * board/contest_board/src/board_spiflash.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>

#include "espressif/esp_spiflash.h"
#include "espressif/esp_spiflash_mtd.h"

#include "board_spiflash.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BOARD_FLASH_SIZE (16u * 1024u * 1024u)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int board_spiflash_initialize(void)
{
  struct mtd_dev_s *mtd;
  uint32_t offset = CONFIG_ESPRESSIF_STORAGE_MTD_OFFSET;
  uint32_t size   = CONFIG_ESPRESSIF_STORAGE_MTD_SIZE;
  int ret;

  /* The GD25Q128ESIG is 16 MiB.  Reject a wrapped or out-of-range
   * partition before exposing an erase-capable device.
   */

  if (size == 0 || offset >= BOARD_FLASH_SIZE ||
      size > BOARD_FLASH_SIZE - offset)
    {
      syslog(LOG_ERR, "ERROR: unsafe SPI Flash MTD range: "
             "offset=0x%08lx size=0x%08lx\n",
             (unsigned long)offset, (unsigned long)size);
      return -EINVAL;
    }

  ret = esp_spiflash_init();
  if (ret < 0)
    {
      return ret;
    }

  mtd = esp_spiflash_alloc_mtdpart(offset, size);
  if (mtd == NULL)
    {
      syslog(LOG_ERR, "ERROR: failed to allocate SPI Flash MTD\n");
      return -ENODEV;
    }

  ret = smart_initialize(0, mtd, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: smart_initialize failed: %d\n", ret);
      return ret;
    }

  ret = nx_mount("/dev/smart0", CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT,
                 "smartfs", 0, NULL);
  if (ret == -ENODEV)
    {
      syslog(LOG_WARNING,
             "WARNING: /dev/smart0 is unformatted; run "
             "mksmartfs /dev/smart0 and reset\n");
      return OK;
    }

  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: failed to mount SPI Flash SmartFS: %d\n",
             ret);
      return ret;
    }

  syslog(LOG_INFO,
         "SPI Flash MTD: 0x%08lx-0x%08lx mounted at %s\n",
         (unsigned long)offset,
         (unsigned long)(offset + size - 1),
         CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT);
  return OK;
}
