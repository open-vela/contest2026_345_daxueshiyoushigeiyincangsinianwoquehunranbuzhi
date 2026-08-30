/****************************************************************************
 * board/contest_board/src/board_touch.h
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

#ifndef __BOARD_CONTEST_BOARD_SRC_BOARD_TOUCH_H
#define __BOARD_CONTEST_BOARD_SRC_BOARD_TOUCH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Touch IC Type Identifiers */

#define CONTEST_TOUCH_TYPE_NONE    0
#define CONTEST_TOUCH_TYPE_GT911   1
#define CONTEST_TOUCH_TYPE_FT5X06  2
#define CONTEST_TOUCH_TYPE_CST816S 3

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Name: board_touch_initialize
 *
 * Description:
 *   Initialize the capacitive touchscreen controller (GT911 / FT5x06 /
 *   CST816S) on I2C0 bus, auto-probe the connected touch IC, and register
 *   the standard NuttX touchscreen upper-half driver at /dev/input0.
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int board_touch_initialize(void);

/****************************************************************************
 * Name: board_touch_get_type
 *
 * Description:
 *   Return the detected touch controller type (GT911 / FT5X06 / CST816S).
 *
 * Returned Value:
 *   CONTEST_TOUCH_TYPE_* enum value.
 *
 ****************************************************************************/

int board_touch_get_type(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONTEST_BOARD_SRC_BOARD_TOUCH_H */
