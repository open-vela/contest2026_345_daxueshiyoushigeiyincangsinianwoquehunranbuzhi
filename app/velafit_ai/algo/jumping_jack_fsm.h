/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/jumping_jack_fsm.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_JUMPING_JACK_FSM_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_JUMPING_JACK_FSM_H

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
  jumping_jack_state_t state;
  uint32_t total_reps;
  uint32_t valid_reps;
  float max_arm_angle_curr;
  float max_leg_ratio_curr;
  uint32_t rep_start_time_ms;
  uint32_t last_rep_complete_time_ms;
} jumping_jack_fsm_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

void jumping_jack_fsm_init(jumping_jack_fsm_t *fsm);

bool jumping_jack_fsm_update(jumping_jack_fsm_t *fsm,
                             const pose_frame_t *pose,
                             uint32_t timestamp_ms);

void jumping_jack_fsm_reset(jumping_jack_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_JUMPING_JACK_FSM_H */
