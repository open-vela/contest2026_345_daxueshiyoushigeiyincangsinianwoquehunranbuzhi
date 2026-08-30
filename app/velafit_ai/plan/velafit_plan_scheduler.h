/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/plan/velafit_plan_scheduler.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_PLAN_VELAFIT_PLAN_SCHEDULER_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_PLAN_VELAFIT_PLAN_SCHEDULER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "velafit_types.h"
#include "velafit_pipeline.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_MAX_PLAN_STEPS         16
#define VELAFIT_PREPARE_DURATION_SEC   3

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef enum
{
  VELAFIT_TARGET_TIME = 0,
  VELAFIT_TARGET_REPS
} velafit_target_type_t;

typedef enum
{
  VELAFIT_PHASE_PREPARE = 0,
  VELAFIT_PHASE_WORK,
  VELAFIT_PHASE_REST,
  VELAFIT_PHASE_FINISHED
} velafit_plan_phase_t;

typedef struct
{
  char exercise_name[16];
  velafit_target_type_t target_type;
  uint32_t target_value;
  uint32_t rest_duration_sec;
  char step_desc[32];
} velafit_workout_step_t;

typedef struct
{
  char plan_id[32];
  char plan_name[32];
  char plan_desc[64];
  uint32_t num_steps;
  velafit_workout_step_t steps[VELAFIT_MAX_PLAN_STEPS];
} velafit_workout_plan_t;

typedef struct
{
  velafit_workout_plan_t plan;
  uint32_t current_step_idx;
  velafit_plan_phase_t current_phase;
  uint32_t phase_elapsed_ms;
  uint32_t phase_target_ms;
  uint32_t total_elapsed_ms;
  uint32_t total_valid_reps;
  float total_calories_kcal;
  velafit_pipeline_t pipeline;
  bool initialized;
} velafit_plan_scheduler_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int velafit_plan_scheduler_init(velafit_plan_scheduler_t *sched,
                                const velafit_workout_plan_t *plan);

int velafit_plan_scheduler_step(velafit_plan_scheduler_t *sched,
                                const pose_frame_t *pose,
                                uint32_t dt_ms);

bool velafit_plan_scheduler_is_finished(
       const velafit_plan_scheduler_t *sched);

int velafit_plan_scheduler_finish(velafit_plan_scheduler_t *sched,
                                  char *json_buf,
                                  size_t json_max_len);

void velafit_plan_scheduler_deinit(velafit_plan_scheduler_t *sched);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_PLAN_VELAFIT_PLAN_SCHEDULER_H */
