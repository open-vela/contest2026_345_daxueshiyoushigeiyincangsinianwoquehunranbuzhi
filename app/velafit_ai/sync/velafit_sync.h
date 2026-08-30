/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/sync/velafit_sync.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_SYNC_VELAFIT_SYNC_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_SYNC_VELAFIT_SYNC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "velafit_types.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Network sender callback provided by wireless link (ESP32-C6 / WiFi) */

typedef int (*velafit_net_sender_t)(const char *topic,
                                    const uint8_t *payload,
                                    size_t len);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int velafit_sync_init(void);

void velafit_sync_register_sender(velafit_net_sender_t sender);

bool velafit_sync_is_network_ready(void);

int velafit_sync_flush(void);

int velafit_sync_get_pending_count(void);

int velafit_sync_mock_sender(const char *topic,
                             const uint8_t *payload,
                             size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_SYNC_VELAFIT_SYNC_H */
