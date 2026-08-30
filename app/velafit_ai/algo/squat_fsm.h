/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/squat_fsm.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_SQUAT_FSM_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_SQUAT_FSM_H

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
  squat_state_t state;
  uint32_t total_reps;
  uint32_t valid_reps;
  uint32_t shallow_count;
  uint32_t knee_caving_count;
  uint32_t trunk_lean_count;

  /* Current rep tracking */

  float min_knee_angle_curr;
  float min_knee_ankle_ratio_curr;
  float max_trunk_lean_curr;
  uint32_t rep_start_time_ms;
  uint32_t last_rep_complete_time_ms;
  uint32_t last_quality_flags;
} squat_fsm_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

void squat_fsm_init(squat_fsm_t *fsm);

bool squat_fsm_update(squat_fsm_t *fsm,
                      const pose_frame_t *pose,
                      uint32_t timestamp_ms);

void squat_fsm_reset_counters(squat_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_SQUAT_FSM_H */
