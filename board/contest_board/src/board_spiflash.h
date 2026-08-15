/****************************************************************************
 * board/contest_board/src/board_spiflash.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __VENDOR_OPENVELA_BOARDS_CONTEST2026_345_SRC_BOARD_SPIFLASH_H
#define __VENDOR_OPENVELA_BOARDS_CONTEST2026_345_SRC_BOARD_SPIFLASH_H

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_SPIFLASH_SMARTFS
int board_spiflash_initialize(void);
#endif

#endif
