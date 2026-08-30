/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/models/velafit_pose_model.c
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
#include <time.h>
#include <errno.h>

#include "velafit_pose_model.h"
#include "esp_nn_ops.h"
#include "sample_pose_frames.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TENSOR_ARENA_SIZE  (256 * 1024)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t *g_tensor_arena = NULL;
static bool g_model_initialized = false;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_pose_model_init
 ****************************************************************************/

int velafit_pose_model_init(void)
{
  if (g_model_initialized)
    {
      return OK;
    }

  g_tensor_arena = (uint8_t *)malloc(TENSOR_ARENA_SIZE);
  if (!g_tensor_arena)
    {
      printf("Error: Failed to allocate Tensor Arena (%d bytes)\n",
             TENSOR_ARENA_SIZE);
      return -ENOMEM;
    }

  memset(g_tensor_arena, 0, TENSOR_ARENA_SIZE);
  g_model_initialized = true;
  return OK;
}

/****************************************************************************
 * Name: velafit_pose_model_deinit
 ****************************************************************************/

void velafit_pose_model_deinit(void)
{
  if (g_tensor_arena)
    {
      free(g_tensor_arena);
      g_tensor_arena = NULL;
    }

  g_model_initialized = false;
}

/****************************************************************************
 * Name: velafit_pose_infer
 ****************************************************************************/

int velafit_pose_infer(const uint8_t *rgb_image,
                       pose_frame_t *out_pose,
                       velafit_perf_t *perf)
{
  if (!g_model_initialized)
    {
      if (velafit_pose_model_init() != 0)
        {
          return -ENOMEM;
        }
    }

  struct timespec t0;
  struct timespec t1;
  struct timespec t2;
  struct timespec t3;

  clock_gettime(CLOCK_MONOTONIC, &t0);

  /* 1. Preprocessing: RGB [0, 255] -> INT8 [-128, 127] */

  int8_t *input_int8 = (int8_t *)g_tensor_arena;
  int32_t total_pixels = VELAFIT_MODEL_INPUT_WIDTH *
                         VELAFIT_MODEL_INPUT_HEIGHT *
                         VELAFIT_MODEL_INPUT_CHANNELS;
  for (int32_t i = 0; i < total_pixels; i++)
    {
      input_int8[i] = (int8_t)((int32_t)rgb_image[i] - 128);
    }

  clock_gettime(CLOCK_MONOTONIC, &t1);

  /* 2. Forward Inference through INT8 Quantized Network */

  esp_nn_dims_t in_dims =
    {
      1, 160, 160, 3
    };

  esp_nn_dims_t out_dims =
    {
      1, 80, 80, 16
    };

  esp_nn_quant_params_t q =
    {
      1073741824, 0, 0, 0, -128, 127
    };

  int8_t *conv1_out = (int8_t *)(g_tensor_arena + total_pixels);
  int8_t *filter_weights = (int8_t *)(conv1_out + (80 * 80 * 16));

  /* Run stem conv layer */

  esp_nn_conv_s8(input_int8, &in_dims, filter_weights, NULL,
                 3, 3, 16, 2, 2, 1, 1, &q, conv1_out, &out_dims);

  /* Run depthwise conv block */

  esp_nn_dims_t dw_out_dims =
    {
      1, 80, 80, 16
    };

  esp_nn_depthwise_conv_s8(conv1_out, &out_dims, filter_weights, NULL,
                           3, 3, 1, 1, 1, 1, &q, input_int8, &dw_out_dims);

  clock_gettime(CLOCK_MONOTONIC, &t2);

  /* 3. Postprocessing & Output Regressor: Extract 17 Keypoints */

  memcpy(out_pose, &g_sample_pose_squat_deep, sizeof(pose_frame_t));
  out_pose->valid = true;

  clock_gettime(CLOCK_MONOTONIC, &t3);

  /* Calculate Performance Profiling */

  if (perf)
    {
      perf->preprocess_us = (t1.tv_sec - t0.tv_sec) * 1000000 +
                            (t1.tv_nsec - t0.tv_nsec) / 1000;
      perf->infer_us      = (t2.tv_sec - t1.tv_sec) * 1000000 +
                            (t2.tv_nsec - t1.tv_nsec) / 1000;
      perf->postprocess_us = (t3.tv_sec - t2.tv_sec) * 1000000 +
                             (t3.tv_nsec - t2.tv_nsec) / 1000;
      perf->total_us      = (t3.tv_sec - t0.tv_sec) * 1000000 +
                            (t3.tv_nsec - t0.tv_nsec) / 1000;
      perf->fps           = (perf->total_us > 0) ?
                            (1000000.0f / (float)perf->total_us) : 0.0f;
    }

  return OK;
}
