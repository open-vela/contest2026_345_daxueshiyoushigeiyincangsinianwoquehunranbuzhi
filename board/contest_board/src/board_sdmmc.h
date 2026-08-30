/****************************************************************************
 * board/contest_board/src/board_sdmmc.h
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

#ifndef __BOARD_CONTEST_BOARD_SRC_BOARD_SDMMC_H
#define __BOARD_CONTEST_BOARD_SRC_BOARD_SDMMC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Name: board_sdmmc_initialize
 *
 * Description:
 *   Initialize the on-board MicroSD card slot on ESP32-P4 SDMMC Slot 0,
 *   register the /dev/mmcsd0 block device, and auto-mount the FAT32
 *   filesystem at /sdcard.
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int board_sdmmc_initialize(void);

/****************************************************************************
 * Name: board_sdmmc_is_mounted
 *
 * Description:
 *   Check whether the MicroSD card FAT32 filesystem is currently mounted.
 *
 * Returned Value:
 *   true if mounted at /sdcard; false otherwise.
 *
 ****************************************************************************/

bool board_sdmmc_is_mounted(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONTEST_BOARD_SRC_BOARD_SDMMC_H */
