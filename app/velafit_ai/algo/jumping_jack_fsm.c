/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/jumping_jack_fsm.c
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

#include "jumping_jack_fsm.h"
#include "geometry.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define JJ_ARM_CLOSED_THRESH      40.0f
#define JJ_ARM_OPENED_THRESH     130.0f
#define JJ_LEG_CLOSED_RATIO        1.10f
#define JJ_LEG_OPENED_RATIO        1.55f
#define JJ_MIN_REP_DURATION_MS     350

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: jumping_jack_fsm_init
 ****************************************************************************/

void jumping_jack_fsm_init(jumping_jack_fsm_t *fsm)
{
  fsm->state = JJ_STATE_CLOSED;
  fsm->total_reps = 0;
  fsm->valid_reps = 0;
  fsm->max_arm_angle_curr = 0.0f;
  fsm->max_leg_ratio_curr = 0.0f;
  fsm->rep_start_time_ms = 0;
  fsm->last_rep_complete_time_ms = 0;
}

/****************************************************************************
 * Name: jumping_jack_fsm_reset
 ****************************************************************************/

void jumping_jack_fsm_reset(jumping_jack_fsm_t *fsm)
{
  jumping_jack_fsm_init(fsm);
}

/****************************************************************************
 * Name: jumping_jack_fsm_update
 ****************************************************************************/

bool jumping_jack_fsm_update(jumping_jack_fsm_t *fsm,
                             const pose_frame_t *pose,
                             uint32_t timestamp_ms)
{
  if (!pose || !pose->valid)
    {
      return false;
    }

  float arm_l = velafit_get_arm_angle_left(pose);
  float arm_r = velafit_get_arm_angle_right(pose);
  float avg_arm = (arm_l + arm_r) * 0.5f;

  float leg_ratio = velafit_get_leg_spread_ratio(pose);
  bool rep_finished = false;

  switch (fsm->state)
    {
      case JJ_STATE_CLOSED:
        if (avg_arm > JJ_ARM_CLOSED_THRESH + 15.0f ||
            leg_ratio > JJ_LEG_CLOSED_RATIO + 0.15f)
          {
            fsm->state = JJ_STATE_OPENING;
            fsm->rep_start_time_ms = timestamp_ms;
            fsm->max_arm_angle_curr = avg_arm;
            fsm->max_leg_ratio_curr = leg_ratio;
          }
        break;

      case JJ_STATE_OPENING:
        if (avg_arm > fsm->max_arm_angle_curr)
          {
            fsm->max_arm_angle_curr = avg_arm;
          }

        if (leg_ratio > fsm->max_leg_ratio_curr)
          {
            fsm->max_leg_ratio_curr = leg_ratio;
          }

        if (avg_arm >= JJ_ARM_OPENED_THRESH &&
            leg_ratio >= JJ_LEG_OPENED_RATIO)
          {
            fsm->state = JJ_STATE_OPENED;
          }
        else if (avg_arm < fsm->max_arm_angle_curr - 15.0f)
          {
            /* Turnaround early */

            fsm->state = JJ_STATE_CLOSING;
          }
        break;

      case JJ_STATE_OPENED:
        if (avg_arm > fsm->max_arm_angle_curr)
          {
            fsm->max_arm_angle_curr = avg_arm;
          }

        if (leg_ratio > fsm->max_leg_ratio_curr)
          {
            fsm->max_leg_ratio_curr = leg_ratio;
          }

        if (avg_arm < JJ_ARM_OPENED_THRESH - 15.0f ||
            leg_ratio < JJ_LEG_OPENED_RATIO - 0.2f)
          {
            fsm->state = JJ_STATE_CLOSING;
          }
        else
          {
            break;
          }

      case JJ_STATE_CLOSING:
        if (avg_arm < JJ_ARM_CLOSED_THRESH &&
            leg_ratio < JJ_LEG_CLOSED_RATIO)
          {
            uint32_t duration = timestamp_ms - fsm->rep_start_time_ms;
            if (duration >= JJ_MIN_REP_DURATION_MS)
              {
                fsm->total_reps++;
                if (fsm->max_arm_angle_curr >= JJ_ARM_OPENED_THRESH &&
                    fsm->max_leg_ratio_curr >= JJ_LEG_OPENED_RATIO)
                  {
                    fsm->valid_reps++;
                  }

                fsm->last_rep_complete_time_ms = timestamp_ms;
                rep_finished = true;
              }

            fsm->state = JJ_STATE_CLOSED;
            fsm->max_arm_angle_curr = 0.0f;
            fsm->max_leg_ratio_curr = 0.0f;
          }
        break;
    }

  return rep_finished;
}
