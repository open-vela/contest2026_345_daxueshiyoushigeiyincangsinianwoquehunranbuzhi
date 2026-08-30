/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/include/velafit_mimo_protocol.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_INCLUDE_VELAFIT_MIMO_PROTOCOL_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_INCLUDE_VELAFIT_MIMO_PROTOCOL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_MIMO_MODEL_ASR         "mimo-v2.5-asr"
#define VELAFIT_MIMO_MODEL_PRO         "mimo-v2.5-pro"
#define VELAFIT_MIMO_MODEL_DIALOGUE    "mimo-v2.5"
#define VELAFIT_MIMO_MODEL_TTS_CLONE   "mimo-v2.5-tts-voiceclone"
#define VELAFIT_MIMO_MODEL_TTS         "mimo-v2.5-tts"

#define VELAFIT_MAX_PRESCRIPTION_LEN   512
#define VELAFIT_MAX_TEXT_LEN           256

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef enum
{
  VELAFIT_MIMO_MSG_ASR_REQUEST = 0,
  VELAFIT_MIMO_MSG_ASR_RESPONSE,
  VELAFIT_MIMO_MSG_WORKOUT_INGEST,
  VELAFIT_MIMO_MSG_PRESCRIPTION_RESPONSE,
  VELAFIT_MIMO_MSG_TTS_REQUEST,
  VELAFIT_MIMO_MSG_TTS_RESPONSE
} velafit_mimo_msg_type_t;

typedef struct
{
  char session_id[32];
  char exercise_type[16];
  uint32_t score_overall;
  uint32_t score_accuracy;
  uint32_t score_stamina;
  char primary_fault[32];
  char coach_commentary[VELAFIT_MAX_PRESCRIPTION_LEN];
  char target_muscle_focus[64];
  char next_recommended_action[128];
  bool requires_tts_playback;
} velafit_mimo_prescription_t;

typedef struct
{
  char session_id[32];
  char voice_text[VELAFIT_MAX_TEXT_LEN];
  float confidence;
  bool command_recognized;
  char action_target[32];
} velafit_mimo_asr_result_t;

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_INCLUDE_VELAFIT_MIMO_PROTOCOL_H */
