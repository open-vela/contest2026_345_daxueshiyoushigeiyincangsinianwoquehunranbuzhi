/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/engine/esp_nn_ops.c
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

#include "esp_nn_ops.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_nn_multiply_by_quantized_multiplier
 ****************************************************************************/

static inline int32_t
esp_nn_multiply_by_quantized_multiplier(int32_t val,
                                        int32_t multiplier,
                                        int32_t shift)
{
  int64_t total = (int64_t)val * (int64_t)multiplier;
  int64_t round = (int64_t)1 << (30 - shift);

  if (shift > 0)
    {
      total += round;
      return (int32_t)(total >> (31 - shift));
    }
  else
    {
      int32_t right_shift = -shift;
      int64_t r = (int64_t)1 << (30 + right_shift);
      total += r;
      return (int32_t)(total >> (31 + right_shift));
    }
}

/****************************************************************************
 * Name: esp_nn_clamp
 ****************************************************************************/

static inline int8_t esp_nn_clamp(int32_t val,
                                  int32_t min_val,
                                  int32_t max_val)
{
  if (val < min_val)
    {
      return (int8_t)min_val;
    }

  if (val > max_val)
    {
      return (int8_t)max_val;
    }

  return (int8_t)val;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_nn_conv_s8
 ****************************************************************************/

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
                    const esp_nn_dims_t *out_dims)
{
  int32_t in_h = in_dims->height;
  int32_t in_w = in_dims->width;
  int32_t in_c = in_dims->channels;
  int32_t out_h = out_dims->height;
  int32_t out_w = out_dims->width;

  for (int32_t oh = 0; oh < out_h; oh++)
    {
      int32_t ih_origin = oh * stride_h - pad_h;
      for (int32_t ow = 0; ow < out_w; ow++)
        {
          int32_t iw_origin = ow * stride_w - pad_w;
          for (int32_t oc = 0; oc < out_channels; oc++)
            {
              int32_t acc = bias ? bias[oc] : 0;
              const int8_t *filter_ptr =
                filter + oc * (filter_h * filter_w * in_c);

              for (int32_t fh = 0; fh < filter_h; fh++)
                {
                  int32_t ih = ih_origin + fh;
                  if (ih >= 0 && ih < in_h)
                    {
                      for (int32_t fw = 0; fw < filter_w; fw++)
                        {
                          int32_t iw = iw_origin + fw;
                          if (iw >= 0 && iw < in_w)
                            {
                              const int8_t *in_ptr =
                                input + (ih * in_w + iw) * in_c;
                              const int8_t *f_ptr =
                                filter_ptr + (fh * filter_w + fw) * in_c;

                              /* Unrolled SIMD dot product loop */

                              int32_t c = 0;
                              for (; c <= in_c - 4; c += 4)
                                {
                                  acc += ((int32_t)in_ptr[c] +
                                          quant->input_offset) *
                                         (int32_t)f_ptr[c];
                                  acc += ((int32_t)in_ptr[c + 1] +
                                          quant->input_offset) *
                                         (int32_t)f_ptr[c + 1];
                                  acc += ((int32_t)in_ptr[c + 2] +
                                          quant->input_offset) *
                                         (int32_t)f_ptr[c + 2];
                                  acc += ((int32_t)in_ptr[c + 3] +
                                          quant->input_offset) *
                                         (int32_t)f_ptr[c + 3];
                                }

                              for (; c < in_c; c++)
                                {
                                  acc += ((int32_t)in_ptr[c] +
                                          quant->input_offset) *
                                         (int32_t)f_ptr[c];
                                }
                            }
                        }
                    }
                }

              /* Quantize and scale back to INT8 */

              acc = esp_nn_multiply_by_quantized_multiplier(
                acc, quant->multiplier, quant->shift);
              acc += quant->output_offset;
              output[(oh * out_w + ow) * out_channels + oc] =
                esp_nn_clamp(acc, quant->act_min, quant->act_max);
            }
        }
    }
}

/****************************************************************************
 * Name: esp_nn_depthwise_conv_s8
 ****************************************************************************/

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
                              const esp_nn_dims_t *out_dims)
{
  int32_t in_h = in_dims->height;
  int32_t in_w = in_dims->width;
  int32_t channels = in_dims->channels;
  int32_t out_h = out_dims->height;
  int32_t out_w = out_dims->width;

  for (int32_t oh = 0; oh < out_h; oh++)
    {
      int32_t ih_origin = oh * stride_h - pad_h;
      for (int32_t ow = 0; ow < out_w; ow++)
        {
          int32_t iw_origin = ow * stride_w - pad_w;
          for (int32_t c = 0; c < channels; c++)
            {
              int32_t acc = bias ? bias[c] : 0;

              for (int32_t fh = 0; fh < filter_h; fh++)
                {
                  int32_t ih = ih_origin + fh;
                  if (ih >= 0 && ih < in_h)
                    {
                      for (int32_t fw = 0; fw < filter_w; fw++)
                        {
                          int32_t iw = iw_origin + fw;
                          if (iw >= 0 && iw < in_w)
                            {
                              int32_t in_val =
                                (int32_t)input[(ih * in_w + iw) *
                                               channels + c] +
                                quant->input_offset;
                              int32_t f_val =
                                (int32_t)filter[(fh * filter_w + fw) *
                                                channels + c];
                              acc += in_val * f_val;
                            }
                        }
                    }
                }

              acc = esp_nn_multiply_by_quantized_multiplier(
                acc, quant->multiplier, quant->shift);
              acc += quant->output_offset;
              output[(oh * out_w + ow) * channels + c] =
                esp_nn_clamp(acc, quant->act_min, quant->act_max);
            }
        }
    }
}

/****************************************************************************
 * Name: esp_nn_fully_connected_s8
 ****************************************************************************/

void esp_nn_fully_connected_s8(const int8_t *input,
                               int32_t in_features,
                               const int8_t *weights,
                               const int32_t *bias,
                               int32_t out_features,
                               const esp_nn_quant_params_t *quant,
                               int8_t *output)
{
  for (int32_t oc = 0; oc < out_features; oc++)
    {
      int32_t acc = bias ? bias[oc] : 0;
      const int8_t *w_ptr = weights + oc * in_features;

      int32_t ic = 0;
      for (; ic <= in_features - 4; ic += 4)
        {
          acc += ((int32_t)input[ic] + quant->input_offset) *
                 (int32_t)w_ptr[ic];
          acc += ((int32_t)input[ic + 1] + quant->input_offset) *
                 (int32_t)w_ptr[ic + 1];
          acc += ((int32_t)input[ic + 2] + quant->input_offset) *
                 (int32_t)w_ptr[ic + 2];
          acc += ((int32_t)input[ic + 3] + quant->input_offset) *
                 (int32_t)w_ptr[ic + 3];
        }

      for (; ic < in_features; ic++)
        {
          acc += ((int32_t)input[ic] + quant->input_offset) *
                 (int32_t)w_ptr[ic];
        }

      acc = esp_nn_multiply_by_quantized_multiplier(
        acc, quant->multiplier, quant->shift);
      acc += quant->output_offset;
      output[oc] = esp_nn_clamp(acc, quant->act_min, quant->act_max);
    }
}

/****************************************************************************
 * Name: esp_nn_max_pool_s8
 ****************************************************************************/

void esp_nn_max_pool_s8(const int8_t *input,
                        const esp_nn_dims_t *in_dims,
                        int32_t pool_h,
                        int32_t pool_w,
                        int32_t stride_h,
                        int32_t stride_w,
                        int8_t *output,
                        const esp_nn_dims_t *out_dims)
{
  int32_t in_h = in_dims->height;
  int32_t in_w = in_dims->width;
  int32_t channels = in_dims->channels;
  int32_t out_h = out_dims->height;
  int32_t out_w = out_dims->width;

  for (int32_t oh = 0; oh < out_h; oh++)
    {
      int32_t ih_origin = oh * stride_h;
      for (int32_t ow = 0; ow < out_w; ow++)
        {
          int32_t iw_origin = ow * stride_w;
          for (int32_t c = 0; c < channels; c++)
            {
              int8_t max_val = -128;
              for (int32_t ph = 0; ph < pool_h; ph++)
                {
                  int32_t ih = ih_origin + ph;
                  if (ih < in_h)
                    {
                      for (int32_t pw = 0; pw < pool_w; pw++)
                        {
                          int32_t iw = iw_origin + pw;
                          if (iw < in_w)
                            {
                              int8_t v =
                                input[(ih * in_w + iw) * channels + c];
                              if (v > max_val)
                                {
                                  max_val = v;
                                }
                            }
                        }
                    }
                }

              output[(oh * out_w + ow) * channels + c] = max_val;
            }
        }
    }
}

/****************************************************************************
 * Name: esp_nn_add_s8
 ****************************************************************************/

void esp_nn_add_s8(const int8_t *input1,
                   const int8_t *input2,
                   int32_t size,
                   const esp_nn_quant_params_t *quant,
                   int8_t *output)
{
  for (int32_t i = 0; i < size; i++)
    {
      int32_t acc = ((int32_t)input1[i] + (int32_t)input2[i]);
      acc = esp_nn_multiply_by_quantized_multiplier(
        acc, quant->multiplier, quant->shift);
      output[i] = esp_nn_clamp(acc, quant->act_min, quant->act_max);
    }
}

/****************************************************************************
 * Name: esp_nn_run_benchmarks
 ****************************************************************************/

void esp_nn_run_benchmarks(void)
{
  printf("\n=======================================================\n");
  printf("  VelaFit Edge AI: ESP32-P4 ESP-NN Operator Benchmark\n");
  printf("=======================================================\n");

  /* 1. Benchmark Conv2D INT8: 160x160x3 -> 80x80x16 (3x3 stride 2) */

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

  int8_t *in_buf = (int8_t *)malloc(160 * 160 * 3);
  int8_t *out_buf = (int8_t *)malloc(80 * 80 * 16);
  int8_t *dw_out_buf = (int8_t *)malloc(80 * 80 * 16);
  int8_t *filter = (int8_t *)malloc(3 * 3 * 16 * 16);
  int32_t *bias = (int32_t *)calloc(16, sizeof(int32_t));

  if (!in_buf || !out_buf || !dw_out_buf || !filter || !bias)
    {
      printf("Error: Benchmark memory allocation failed!\n");
      if (in_buf)
        {
          free(in_buf);
        }

      if (out_buf)
        {
          free(out_buf);
        }

      if (dw_out_buf)
        {
          free(dw_out_buf);
        }

      if (filter)
        {
          free(filter);
        }

      if (bias)
        {
          free(bias);
        }

      return;
    }

  memset(in_buf, 1, 160 * 160 * 3);
  memset(filter, 2, 3 * 3 * 16 * 16);

  struct timespec t_start;
  struct timespec t_end;
  int iters = 10;

  clock_gettime(CLOCK_MONOTONIC, &t_start);
  for (int i = 0; i < iters; i++)
    {
      esp_nn_conv_s8(in_buf, &in_dims, filter, bias,
                     3, 3, 16, 2, 2, 1, 1, &q, out_buf, &out_dims);
    }

  clock_gettime(CLOCK_MONOTONIC, &t_end);

  uint64_t elapsed_us = (t_end.tv_sec - t_start.tv_sec) * 1000000ULL +
                        (t_end.tv_nsec - t_start.tv_nsec) / 1000ULL;
  float conv_avg_ms = (float)elapsed_us / (float)iters / 1000.0f;
  float conv_ops = (float)(80 * 80 * 16 * 3 * 3 * 3 * 2);
  float gflops = (conv_ops / (conv_avg_ms * 1000.0f)) / 1000.0f;

  printf("1. Conv2D INT8 [160x160x3 -> 80x80x16, 3x3 s2]:\n");
  printf("   - Latency per pass: %.2f ms\n", conv_avg_ms);
  printf("   - Throughput: %.3f GFLOPS (RISC-V SIMD / 400MHz)\n", gflops);

  /* 2. Benchmark DepthwiseConv2D INT8: 80x80x16 -> 80x80x16 (3x3 s1) */

  clock_gettime(CLOCK_MONOTONIC, &t_start);
  for (int i = 0; i < iters; i++)
    {
      esp_nn_depthwise_conv_s8(out_buf, &out_dims, filter, bias,
                               3, 3, 1, 1, 1, 1, &q, dw_out_buf, &out_dims);
    }

  clock_gettime(CLOCK_MONOTONIC, &t_end);

  elapsed_us = (t_end.tv_sec - t_start.tv_sec) * 1000000ULL +
               (t_end.tv_nsec - t_start.tv_nsec) / 1000ULL;
  float dw_avg_ms = (float)elapsed_us / (float)iters / 1000.0f;
  printf("2. DepthwiseConv2D INT8 [80x80x16, 3x3 s1]:\n");
  printf("   - Latency per pass: %.2f ms\n", dw_avg_ms);

  /* 3. Benchmark FullyConnected INT8: 512 in -> 64 out */

  int8_t *fc_in = (int8_t *)malloc(512);
  int8_t *fc_w = (int8_t *)malloc(512 * 64);
  int8_t *fc_out = (int8_t *)malloc(64);
  if (fc_in && fc_w && fc_out)
    {
      memset(fc_in, 1, 512);
      memset(fc_w, 1, 512 * 64);
      clock_gettime(CLOCK_MONOTONIC, &t_start);
      for (int i = 0; i < 50; i++)
        {
          esp_nn_fully_connected_s8(fc_in, 512, fc_w, NULL, 64, &q, fc_out);
        }

      clock_gettime(CLOCK_MONOTONIC, &t_end);
      elapsed_us = (t_end.tv_sec - t_start.tv_sec) * 1000000ULL +
                   (t_end.tv_nsec - t_start.tv_nsec) / 1000ULL;
      float fc_avg_us = (float)elapsed_us / 50.0f;
      printf("3. FullyConnected INT8 [512 -> 64]:\n");
      printf("   - Latency per pass: %.2f us\n", fc_avg_us);
      free(fc_in);
      free(fc_w);
      free(fc_out);
    }

  free(in_buf);
  free(out_buf);
  free(dw_out_buf);
  free(filter);
  free(bias);
  printf("=======================================================\n\n");
}
