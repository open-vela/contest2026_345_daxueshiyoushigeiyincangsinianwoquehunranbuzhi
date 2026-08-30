/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/velafit_kws.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_VELAFIT_KWS_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_VELAFIT_KWS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_KWS_KEYWORD_PRIMARY     "nihao_openvela"    /* 你好，openvela */
#define VELAFIT_KWS_KEYWORD_EN          "hello_openvela"    /* Hello, openvela */
#define VELAFIT_KWS_KEYWORD_SECONDARY   "velafit"           /* VelaFit */

#define VELAFIT_KWS_SAMPLE_RATE         16000
#define VELAFIT_KWS_FRAME_SAMPLES       320   /* 20 ms */

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef enum
{
  VELAFIT_KWS_STATE_IDLE = 0,
  VELAFIT_KWS_STATE_LISTENING,
  VELAFIT_KWS_STATE_TRIGGERED,
  VELAFIT_KWS_STATE_CAPTURING_CMD
} velafit_kws_state_t;

typedef void (*velafit_kws_callback_t)(const char *keyword,
                                       float confidence);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int velafit_kws_init(velafit_kws_callback_t cb);

void velafit_kws_deinit(void);

int velafit_kws_feed_pcm(const int16_t *pcm_samples,
                         size_t count);

velafit_kws_state_t velafit_kws_get_state(void);

bool velafit_kws_is_triggered(void);

void velafit_kws_reset(void);

int velafit_kws_run_simulation(const char *test_mode);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_VELAFIT_KWS_H */
