/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/audio/velafit_audio_cue.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_AUDIO_VELAFIT_AUDIO_CUE_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_AUDIO_VELAFIT_AUDIO_CUE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef enum
{
  VELAFIT_AUDIO_CUE_START = 0,
  VELAFIT_AUDIO_CUE_REP_COUNT,
  VELAFIT_AUDIO_CUE_WARN_SHALLOW,
  VELAFIT_AUDIO_CUE_WARN_VALGUS,
  VELAFIT_AUDIO_CUE_WARN_LEAN,
  VELAFIT_AUDIO_CUE_WARN_SAG,
  VELAFIT_AUDIO_CUE_WARN_PIKE,
  VELAFIT_AUDIO_CUE_COUNTDOWN,
  VELAFIT_AUDIO_CUE_REST,
  VELAFIT_AUDIO_CUE_WHISTLE,
  VELAFIT_AUDIO_CUE_FINISH,
  VELAFIT_AUDIO_CUE_MAX
} velafit_audio_cue_type_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int velafit_audio_cue_init(void);

void velafit_audio_cue_deinit(void);

int velafit_audio_cue_play(velafit_audio_cue_type_t cue);

const char *velafit_audio_cue_name(velafit_audio_cue_type_t cue);

const char *velafit_audio_cue_desc(velafit_audio_cue_type_t cue);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_AUDIO_VELAFIT_AUDIO_CUE_H */
