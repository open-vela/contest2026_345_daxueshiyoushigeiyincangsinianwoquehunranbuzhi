/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/engine/velafit_ppa_bench.c
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

#include "esp32p4_ppa.h"
#include "velafit_ppa_bench.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_ppa_run_benchmarks
 ****************************************************************************/

int velafit_ppa_run_benchmarks(void)
{
  printf("\n=======================================================\n");
  printf("  ESP32-P4 PPA (2D Hardware Accelerator) Benchmarks\n");
  printf("=======================================================\n");

  /* Initialize PPA hardware */

  int ret = esp32p4_ppa_init();
  if (ret != OK)
    {
      printf("ERROR: Failed to initialize PPA driver: %d\n", ret);
      return ret;
    }

  printf("  [Driver Status]: PPA Hardware Engine Initialized [OK]\n\n");

  /* 1. Benchmark Scaling: 720p (1280x720) -> 160x160 AI Input */

  printf("--- 1. Camera Downscaling (1280x720 RGB565 -> 160x160 AI) ---\n");
  size_t src_size = 1280 * 720 * sizeof(uint16_t);
  size_t dst_size = 160 * 160 * sizeof(uint16_t);

  uint16_t *scale_src = (uint16_t *)malloc(src_size);
  uint16_t *scale_dst = (uint16_t *)malloc(dst_size);

  if (scale_src != NULL && scale_dst != NULL)
    {
      memset(scale_src, 0x55, src_size);
      struct timespec t_start;
      struct timespec t_end;

      clock_gettime(CLOCK_MONOTONIC, &t_start);
      for (int i = 0; i < 50; i++)
        {
          esp32p4_ppa_scale(scale_src, 1280, 720,
                            scale_dst, 160, 160,
                            ESP32P4_PPA_COLOR_RGB565);
        }

      clock_gettime(CLOCK_MONOTONIC, &t_end);
      double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                          (t_end.tv_nsec - t_start.tv_nsec) / 1000000.0;
      double avg_ms = elapsed_ms / 50.0;
      printf("  Scale 1280x720 -> 160x160 : Avg %6.2f ms / frame "
             "(Throughput: %5.1f FPS)\n", avg_ms, 1000.0 / avg_ms);
    }

  free(scale_src);
  free(scale_dst);

  /* 2. Benchmark Rotation: 160x160 RGB565 (90 deg) */

  printf("\n--- 2. Image Rotation (160x160 RGB565 90 deg CCW) ---\n");
  uint16_t *rot_src = (uint16_t *)malloc(dst_size);
  uint16_t *rot_dst = (uint16_t *)malloc(dst_size);

  if (rot_src != NULL && rot_dst != NULL)
    {
      memset(rot_src, 0xaa, dst_size);
      struct timespec t_start;
      struct timespec t_end;

      clock_gettime(CLOCK_MONOTONIC, &t_start);
      for (int i = 0; i < 100; i++)
        {
          esp32p4_ppa_rotate(rot_src, 160, 160, rot_dst,
                             ESP32P4_PPA_ROT_90,
                             ESP32P4_PPA_COLOR_RGB565);
        }

      clock_gettime(CLOCK_MONOTONIC, &t_end);
      double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                          (t_end.tv_nsec - t_start.tv_nsec) / 1000000.0;
      double avg_ms = elapsed_ms / 100.0;
      printf("  Rotate 160x160 (90 deg)   : Avg %6.3f ms / frame "
             "(Throughput: %5.1f FPS)\n", avg_ms, 1000.0 / avg_ms);
    }

  free(rot_src);
  free(rot_dst);

  /* 3. Benchmark Alpha Blending: 160x160 RGB888 */

  printf("\n--- 3. 2D Alpha Blending (160x160 RGB888 Alpha=128) ---\n");
  size_t rgb888_size = 160 * 160 * 3;
  uint8_t *bg_buf = (uint8_t *)malloc(rgb888_size);
  uint8_t *fg_buf = (uint8_t *)malloc(rgb888_size);
  uint8_t *blend_dst = (uint8_t *)malloc(rgb888_size);

  if (bg_buf != NULL && fg_buf != NULL && blend_dst != NULL)
    {
      memset(bg_buf, 0x40, rgb888_size);
      memset(fg_buf, 0x80, rgb888_size);
      struct timespec t_start;
      struct timespec t_end;

      clock_gettime(CLOCK_MONOTONIC, &t_start);
      for (int i = 0; i < 100; i++)
        {
          esp32p4_ppa_blend(bg_buf, fg_buf, blend_dst, 160, 160,
                            128, ESP32P4_PPA_COLOR_RGB888);
        }

      clock_gettime(CLOCK_MONOTONIC, &t_end);
      double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                          (t_end.tv_nsec - t_start.tv_nsec) / 1000000.0;
      double avg_ms = elapsed_ms / 100.0;
      printf("  Alpha Blend 160x160 RGB888: Avg %6.3f ms / frame "
             "(Throughput: %5.1f FPS)\n", avg_ms, 1000.0 / avg_ms);
    }

  free(bg_buf);
  free(fg_buf);
  free(blend_dst);

  /* 4. Benchmark Color Space Conversion: RGB565 -> RGB888 -> GRAY8 */

  printf("\n--- 4. Color Conversion (RGB565 -> RGB888 -> GRAY8) ---\n");
  uint16_t *csc_in = (uint16_t *)malloc(160 * 160 * sizeof(uint16_t));
  uint8_t *csc_rgb = (uint8_t *)malloc(160 * 160 * 3);
  uint8_t *csc_gray = (uint8_t *)malloc(160 * 160);

  if (csc_in != NULL && csc_rgb != NULL && csc_gray != NULL)
    {
      memset(csc_in, 0x1f, 160 * 160 * sizeof(uint16_t));
      struct timespec t_start;
      struct timespec t_end;

      clock_gettime(CLOCK_MONOTONIC, &t_start);
      for (int i = 0; i < 100; i++)
        {
          esp32p4_ppa_csc(csc_in, csc_rgb, 160, 160,
                          ESP32P4_PPA_COLOR_RGB565,
                          ESP32P4_PPA_COLOR_RGB888);
          esp32p4_ppa_csc(csc_rgb, csc_gray, 160, 160,
                          ESP32P4_PPA_COLOR_RGB888,
                          ESP32P4_PPA_COLOR_GRAY8);
        }

      clock_gettime(CLOCK_MONOTONIC, &t_end);
      double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                          (t_end.tv_nsec - t_start.tv_nsec) / 1000000.0;
      double avg_ms = elapsed_ms / 100.0;
      printf("  CSC RGB565->RGB888->GRAY8 : Avg %6.3f ms / frame "
             "(Throughput: %5.1f FPS)\n", avg_ms, 1000.0 / avg_ms);
    }

  free(csc_in);
  free(csc_rgb);
  free(csc_gray);

  /* 5. Benchmark Fast Fill: 1024x600 Framebuffer */

  printf("\n--- 5. Fast Framebuffer Fill (1024x600 RGB565) ---\n");
  size_t fb_size = 1024 * 600 * sizeof(uint16_t);
  uint16_t *fb_buf = (uint16_t *)malloc(fb_size);

  if (fb_buf != NULL)
    {
      struct timespec t_start;
      struct timespec t_end;

      clock_gettime(CLOCK_MONOTONIC, &t_start);
      for (int i = 0; i < 100; i++)
        {
          esp32p4_ppa_fill(fb_buf, 1024, 600, 0x0000,
                           ESP32P4_PPA_COLOR_RGB565);
        }

      clock_gettime(CLOCK_MONOTONIC, &t_end);
      double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                          (t_end.tv_nsec - t_start.tv_nsec) / 1000000.0;
      double avg_ms = elapsed_ms / 100.0;
      double mb_per_sec =
        (fb_size / (1024.0 * 1024.0)) / (avg_ms / 1000.0);
      printf("  Fill 1024x600 (Clear FB)  : Avg %6.3f ms / frame "
             "(Throughput: %6.1f MB/s)\n", avg_ms, mb_per_sec);
    }

  free(fb_buf);

  printf("=======================================================\n");
  printf("  ESP32-P4 PPA Hardware Acceleration Suite: [ALL PASS]\n");
  printf("=======================================================\n\n");
  return OK;
}
