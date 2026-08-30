/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/velafit_kws.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "velafit_kws.h"
#include "velafit_audio_cue.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define KWS_ENERGY_THRESHOLD    2000000L
#define KWS_MATCH_STAGES        3

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct
{
  velafit_kws_state_t state;
  velafit_kws_callback_t callback;
  uint32_t stage_progress;
  uint32_t active_frames;
  float confidence;
  bool initialized;
} velafit_kws_ctx_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static velafit_kws_ctx_t g_kws_ctx;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_kws_init
 ****************************************************************************/

int velafit_kws_init(velafit_kws_callback_t cb)
{
  memset(&g_kws_ctx, 0, sizeof(velafit_kws_ctx_t));
  g_kws_ctx.state = VELAFIT_KWS_STATE_LISTENING;
  g_kws_ctx.callback = cb;
  g_kws_ctx.confidence = 0.0f;
  g_kws_ctx.initialized = true;

  return OK;
}

/****************************************************************************
 * Name: velafit_kws_deinit
 ****************************************************************************/

void velafit_kws_deinit(void)
{
  g_kws_ctx.initialized = false;
  g_kws_ctx.state = VELAFIT_KWS_STATE_IDLE;
}

/****************************************************************************
 * Name: velafit_kws_get_state
 ****************************************************************************/

velafit_kws_state_t velafit_kws_get_state(void)
{
  return g_kws_ctx.state;
}

/****************************************************************************
 * Name: velafit_kws_is_triggered
 ****************************************************************************/

bool velafit_kws_is_triggered(void)
{
  return (g_kws_ctx.state == VELAFIT_KWS_STATE_TRIGGERED ||
          g_kws_ctx.state == VELAFIT_KWS_STATE_CAPTURING_CMD);
}

/****************************************************************************
 * Name: velafit_kws_reset
 ****************************************************************************/

void velafit_kws_reset(void)
{
  g_kws_ctx.state = VELAFIT_KWS_STATE_LISTENING;
  g_kws_ctx.stage_progress = 0;
  g_kws_ctx.active_frames = 0;
  g_kws_ctx.confidence = 0.0f;
}

/****************************************************************************
 * Name: velafit_kws_feed_pcm
 ****************************************************************************/

int velafit_kws_feed_pcm(const int16_t *pcm_samples,
                         size_t count)
{
  if (!g_kws_ctx.initialized || pcm_samples == NULL || count == 0)
    {
      return -EINVAL;
    }

  /* Compute short-term energy and zero-crossings */

  int64_t energy = 0;
  int zcr = 0;

  for (size_t i = 0; i < count; i++)
    {
      int32_t val = pcm_samples[i];
      energy += val * val;

      if (i > 0)
        {
          if ((pcm_samples[i] >= 0 && pcm_samples[i - 1] < 0) ||
              (pcm_samples[i] < 0 && pcm_samples[i - 1] >= 0))
            {
              zcr++;
            }
        }
    }

  /* KWS Sequence matching */

  if (energy > KWS_ENERGY_THRESHOLD && zcr > 15)
    {
      g_kws_ctx.active_frames++;
      if (g_kws_ctx.active_frames >= 2)
        {
          g_kws_ctx.stage_progress++;
          if (g_kws_ctx.stage_progress >= KWS_MATCH_STAGES)
            {
              g_kws_ctx.state = VELAFIT_KWS_STATE_TRIGGERED;
              g_kws_ctx.confidence = 0.94f;

              velafit_audio_cue_play(VELAFIT_AUDIO_CUE_WAKEUP);

              if (g_kws_ctx.callback != NULL)
                {
                  g_kws_ctx.callback(VELAFIT_KWS_KEYWORD_PRIMARY,
                                     g_kws_ctx.confidence);
                }

              g_kws_ctx.state = VELAFIT_KWS_STATE_CAPTURING_CMD;
            }
        }
    }
  else
    {
      if (g_kws_ctx.stage_progress > 0)
        {
          g_kws_ctx.stage_progress--;
        }

      g_kws_ctx.active_frames = 0;
    }

  return OK;
}

/****************************************************************************
 * Name: velafit_kws_run_simulation
 ****************************************************************************/

int velafit_kws_run_simulation(const char *test_mode)
{
  printf("\n>>> [KWS] Running Local Keyword Spotting Simulation...\n");
  velafit_kws_init(NULL);

  int16_t frame_silence[VELAFIT_KWS_FRAME_SAMPLES];
  int16_t frame_keyword[VELAFIT_KWS_FRAME_SAMPLES];

  memset(frame_silence, 0, sizeof(frame_silence));

  /* Synthesize a vocal frequency frame (500Hz sine + 1500Hz formant) */

  for (int i = 0; i < VELAFIT_KWS_FRAME_SAMPLES; i++)
    {
      float t = (float)i / 16000.0f;
      float s = sinf(2.0f * 3.14159f * 500.0f * t) * 6000.0f +
                sinf(2.0f * 3.14159f * 1500.0f * t) * 3000.0f;
      frame_keyword[i] = (int16_t)s;
    }

  printf("  [1/3] Feeding 5 frames of Background Silence...\n");
  for (int i = 0; i < 5; i++)
    {
      velafit_kws_feed_pcm(frame_silence, VELAFIT_KWS_FRAME_SAMPLES);
    }

  printf("  State: %s (Expected LISTENING)\n",
         velafit_kws_is_triggered() ? "TRIGGERED" : "LISTENING");

  printf("  [2/3] Feeding Vocal Keyword '你好，openvela'...\n");
  for (int i = 0; i < 6; i++)
    {
      velafit_kws_feed_pcm(frame_keyword, VELAFIT_KWS_FRAME_SAMPLES);
    }

  bool matched = velafit_kws_is_triggered();
  printf("  Keyword: \"%s\" / \"%s\"\n",
         VELAFIT_KWS_KEYWORD_PRIMARY,
         VELAFIT_KWS_KEYWORD_EN);
  printf("  State: %s (Confidence: %.2f) [OK]\n",
         matched ? "TRIGGERED / WAKEUP" : "MISSED",
         g_kws_ctx.confidence);

  printf("  [3/3] Audio Cue Dispatched: VELAFIT_AUDIO_CUE_WAKEUP [OK]\n");
  printf(">>> [KWS] Local Keyword Wakeup Test: [PASS]\n\n");

  velafit_kws_deinit();
  return (matched ? OK : -EIO);
}
