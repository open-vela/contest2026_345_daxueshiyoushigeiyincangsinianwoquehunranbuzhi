/****************************************************************************
 * board/contest_board/src/board_sdmmc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>

#include <arch/chip/esp32p4_sdmmc.h>

#include "board_sdmmc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CONTEST_SDMMC_SLOT       0
#define CONTEST_SDMMC_DEV        "/dev/mmcsd0"
#define CONTEST_SDMMC_MOUNT      "/sdcard"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_sdcard_mounted = false;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_sdmmc_is_mounted
 ****************************************************************************/

bool board_sdmmc_is_mounted(void)
{
  return g_sdcard_mounted;
}

/****************************************************************************
 * Name: board_sdmmc_initialize
 ****************************************************************************/

int board_sdmmc_initialize(void)
{
  FAR struct sdio_dev_s *sdio;
  int ret;

  /* 1. Initialize ESP32-P4 SDMMC Host Controller on Slot 0 */

  sdio = esp32p4_sdmmc_sdio_initialize(CONTEST_SDMMC_SLOT);
  if (sdio == NULL)
    {
      mcerr("ERROR: Failed to initialize SDMMC Slot %d\n",
            CONTEST_SDMMC_SLOT);
      return -ENODEV;
    }

  /* 2. Bind SDIO Slot to MMCSD Block Driver (/dev/mmcsd0) */

  ret = mmcsd_slotinitialize(CONTEST_SDMMC_SLOT, sdio);
  if (ret < 0)
    {
      mcerr("ERROR: Failed to initialize mmcsd slot %d: %d\n",
            CONTEST_SDMMC_SLOT, ret);
      return ret;
    }

  mcinfo("Registered MicroSD block device at %s\n", CONTEST_SDMMC_DEV);

  /* 3. Auto-Mount FAT32 Filesystem at /sdcard */

#ifdef CONFIG_FS_FAT
  ret = nx_mount(CONTEST_SDMMC_DEV, CONTEST_SDMMC_MOUNT, "vfat", 0, NULL);
  if (ret < 0)
    {
      mcwarn("WARNING: Auto-mount %s to %s failed (%d)\n",
             CONTEST_SDMMC_DEV, CONTEST_SDMMC_MOUNT, ret);
      mcwarn("         Card may need formatting or is not inserted.\n");
      g_sdcard_mounted = false;
      return ret;
    }

  g_sdcard_mounted = true;
  mcinfo("Successfully mounted %s to %s (FAT32)\n",
         CONTEST_SDMMC_DEV, CONTEST_SDMMC_MOUNT);

  /* 4. Ensure media directories exist */

  mkdir("/sdcard/velafit", 0777);
  mkdir("/sdcard/velafit/media", 0777);
  mkdir("/sdcard/velafit/logs", 0777);
#endif

  return OK;
}
