/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/engine/esp_nn_ops.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ENGINE_ESP_NN_OPS_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ENGINE_ESP_NN_OPS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* INT8 Quantized Tensor Dimensions */

typedef struct
{
  int32_t batches;
  int32_t height;
  int32_t width;
  int32_t channels;
} esp_nn_dims_t;

/* Quantization Scale & Shift representation */

typedef struct
{
  int32_t multiplier;
  int32_t shift;
  int32_t input_offset;
  int32_t output_offset;
  int32_t act_min;
  int32_t act_max;
} esp_nn_quant_params_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

void esp_nn_conv_s8(const int8_t *input,
                    const esp_nn_dims_t *in_dims,
                    const int8_t *filter,
                    const int32_t *bias,
                    int32_t filter_h,
                    int32_t filter_w,
                    int32_t out_channels,
                    int32_t stride_h,
                    int32_t stride_w,
                    int32_t pad_h,
                    int32_t pad_w,
                    const esp_nn_quant_params_t *quant,
                    int8_t *output,
                    const esp_nn_dims_t *out_dims);

void esp_nn_depthwise_conv_s8(const int8_t *input,
                              const esp_nn_dims_t *in_dims,
                              const int8_t *filter,
                              const int32_t *bias,
                              int32_t filter_h,
                              int32_t filter_w,
                              int32_t stride_h,
                              int32_t stride_w,
                              int32_t pad_h,
                              int32_t pad_w,
                              const esp_nn_quant_params_t *quant,
                              int8_t *output,
                              const esp_nn_dims_t *out_dims);

void esp_nn_fully_connected_s8(const int8_t *input,
                               int32_t in_features,
                               const int8_t *weights,
                               const int32_t *bias,
                               int32_t out_features,
                               const esp_nn_quant_params_t *quant,
                               int8_t *output);

void esp_nn_max_pool_s8(const int8_t *input,
                        const esp_nn_dims_t *in_dims,
                        int32_t pool_h,
                        int32_t pool_w,
                        int32_t stride_h,
                        int32_t stride_w,
                        int8_t *output,
                        const esp_nn_dims_t *out_dims);

void esp_nn_add_s8(const int8_t *input1,
                   const int8_t *input2,
                   int32_t size,
                   const esp_nn_quant_params_t *quant,
                   int8_t *output);

void esp_nn_run_benchmarks(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ENGINE_ESP_NN_OPS_H */
