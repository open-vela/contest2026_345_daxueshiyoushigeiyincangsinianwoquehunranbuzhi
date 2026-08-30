/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/plank_fsm.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_PLANK_FSM_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_PLANK_FSM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>

#include "velafit_types.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  plank_state_t state;
  uint32_t start_time_ms;
  uint32_t last_update_time_ms;
  uint32_t total_hold_duration_ms;
  uint32_t valid_hold_duration_ms;
  uint32_t hips_sag_duration_ms;
  uint32_t hips_pike_duration_ms;
  uint32_t head_drop_duration_ms;
  uint32_t last_quality_flags;
  float curr_body_line_angle;
} plank_fsm_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

void plank_fsm_init(plank_fsm_t *fsm);

void plank_fsm_reset(plank_fsm_t *fsm);

bool plank_fsm_update(plank_fsm_t *fsm,
                      const pose_frame_t *pose,
                      uint32_t timestamp_ms);

float plank_fsm_get_quality_score(const plank_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_PLANK_FSM_H */
