/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/storage/velafit_storage.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_STORAGE_VELAFIT_STORAGE_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_STORAGE_VELAFIT_STORAGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "velafit_types.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_STORAGE_SDCARD_PATH    "/sdcard/velafit"
#define VELAFIT_STORAGE_DEFAULT_PATH   "/data/velafit"
#define VELAFIT_STORAGE_FALLBACK_PATH  "/tmp/velafit"
#define VELAFIT_STORAGE_MAX_SESSIONS   64

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int velafit_storage_init(const char *base_path);

int velafit_storage_save_session(const velafit_session_record_t *rec,
                                 const char *json_payload);

int velafit_storage_load_session(const char *session_id,
                                 char *json_buf,
                                 size_t max_len);

int velafit_storage_list_sessions(velafit_session_record_t *records,
                                  size_t max_count,
                                  size_t *actual_count);

int velafit_storage_update_status(const char *session_id,
                                  velafit_sync_status_t status);

int velafit_storage_get_summary(uint32_t *total_sessions,
                                uint32_t *pending_count,
                                float *total_calories);

const char *velafit_storage_get_path(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_STORAGE_VELAFIT_STORAGE_H */
