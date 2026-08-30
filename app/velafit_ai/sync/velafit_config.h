/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/sync/velafit_config.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_SYNC_VELAFIT_CONFIG_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_SYNC_VELAFIT_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_CONFIG_DEFAULT_URL      "https://api.mimo.mi.com/v1"
#define VELAFIT_CONFIG_DEFAULT_PRO_MDL  "mimo-v2.5-pro"
#define VELAFIT_CONFIG_DEFAULT_ASR_MDL  "mimo-v2.5-asr"
#define VELAFIT_CONFIG_DEFAULT_TTS_MDL  "mimo-v2.5-tts-voiceclone"

#define VELAFIT_CONFIG_MAX_URL_LEN      128
#define VELAFIT_CONFIG_MAX_KEY_LEN      128
#define VELAFIT_CONFIG_MAX_NAME_LEN     64

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  char base_url[VELAFIT_CONFIG_MAX_URL_LEN];
  char api_key[VELAFIT_CONFIG_MAX_KEY_LEN];
  char pro_model[VELAFIT_CONFIG_MAX_NAME_LEN];
  char asr_model[VELAFIT_CONFIG_MAX_NAME_LEN];
  char tts_model[VELAFIT_CONFIG_MAX_NAME_LEN];
  char wifi_ssid[VELAFIT_CONFIG_MAX_NAME_LEN];
  char wifi_pwd[VELAFIT_CONFIG_MAX_NAME_LEN];
  bool cloud_enabled;
  bool initialized;
} velafit_config_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int velafit_config_init(void);

const velafit_config_t *velafit_config_get(void);

int velafit_config_save(void);

int velafit_config_set_url(const char *url);

int velafit_config_set_key(const char *key);

int velafit_config_set_model(const char *model);

int velafit_config_set_wifi(const char *ssid, const char *pwd);

int velafit_config_reset(void);

void velafit_config_print(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_SYNC_VELAFIT_CONFIG_H */
