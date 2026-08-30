/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/squat_fsm.c
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

#include "squat_fsm.h"
#include "geometry.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SQUAT_ANGLE_STAND_THRESH     155.0f
#define SQUAT_ANGLE_DESCEND_THRESH   140.0f
#define SQUAT_ANGLE_DEEP_TARGET       95.0f
#define SQUAT_ANGLE_SHALLOW_LIMIT    105.0f
#define SQUAT_ANGLE_ASCEND_THRESH    115.0f
#define SQUAT_VALGUS_RATIO_LIMIT       0.72f
#define SQUAT_TRUNK_LEAN_LIMIT        45.0f
#define SQUAT_MIN_REP_DURATION_MS    500

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: squat_fsm_init
 ****************************************************************************/

void squat_fsm_init(squat_fsm_t *fsm)
{
  fsm->state = SQUAT_STATE_STAND;
  fsm->total_reps = 0;
  fsm->valid_reps = 0;
  fsm->shallow_count = 0;
  fsm->knee_caving_count = 0;
  fsm->trunk_lean_count = 0;
  fsm->min_knee_angle_curr = 180.0f;
  fsm->min_knee_ankle_ratio_curr = 1.0f;
  fsm->max_trunk_lean_curr = 0.0f;
  fsm->rep_start_time_ms = 0;
  fsm->last_rep_complete_time_ms = 0;
  fsm->last_quality_flags = SQUAT_QUALITY_OK;
}

/****************************************************************************
 * Name: squat_fsm_reset_counters
 ****************************************************************************/

void squat_fsm_reset_counters(squat_fsm_t *fsm)
{
  squat_fsm_init(fsm);
}

/****************************************************************************
 * Name: squat_fsm_update
 ****************************************************************************/

bool squat_fsm_update(squat_fsm_t *fsm,
                      const pose_frame_t *pose,
                      uint32_t timestamp_ms)
{
  if (!pose || !pose->valid)
    {
      return false;
    }

  float knee_l = velafit_get_knee_angle_left(pose);
  float knee_r = velafit_get_knee_angle_right(pose);
  float avg_knee = (knee_l + knee_r) * 0.5f;

  float knee_ankle_ratio = velafit_get_knee_to_ankle_ratio(pose);
  float trunk_lean = velafit_get_trunk_lean_angle(pose);

  bool rep_finished = false;

  switch (fsm->state)
    {
      case SQUAT_STATE_STAND:
        if (avg_knee < SQUAT_ANGLE_DESCEND_THRESH)
          {
            fsm->state = SQUAT_STATE_DESCENDING;
            fsm->rep_start_time_ms = timestamp_ms;
            fsm->min_knee_angle_curr = avg_knee;
            fsm->min_knee_ankle_ratio_curr = knee_ankle_ratio;
            fsm->max_trunk_lean_curr = trunk_lean;
          }
        break;

      case SQUAT_STATE_DESCENDING:
        if (avg_knee < fsm->min_knee_angle_curr)
          {
            fsm->min_knee_angle_curr = avg_knee;
          }

        if (knee_ankle_ratio < fsm->min_knee_ankle_ratio_curr)
          {
            fsm->min_knee_ankle_ratio_curr = knee_ankle_ratio;
          }

        if (trunk_lean > fsm->max_trunk_lean_curr)
          {
            fsm->max_trunk_lean_curr = trunk_lean;
          }

        if (avg_knee <= SQUAT_ANGLE_DEEP_TARGET)
          {
            fsm->state = SQUAT_STATE_BOTTOM;
          }
        else if (avg_knee > SQUAT_ANGLE_ASCEND_THRESH &&
                 fsm->min_knee_angle_curr < SQUAT_ANGLE_DESCEND_THRESH)
          {
            /* Turnaround early without reaching target depth */

            fsm->state = SQUAT_STATE_ASCENDING;
          }
        else
          {
            break;
          }

      case SQUAT_STATE_BOTTOM:
        if (avg_knee < fsm->min_knee_angle_curr)
          {
            fsm->min_knee_angle_curr = avg_knee;
          }

        if (knee_ankle_ratio < fsm->min_knee_ankle_ratio_curr)
          {
            fsm->min_knee_ankle_ratio_curr = knee_ankle_ratio;
          }

        if (trunk_lean > fsm->max_trunk_lean_curr)
          {
            fsm->max_trunk_lean_curr = trunk_lean;
          }

        if (avg_knee > SQUAT_ANGLE_ASCEND_THRESH)
          {
            fsm->state = SQUAT_STATE_ASCENDING;
          }
        else
          {
            break;
          }

      case SQUAT_STATE_ASCENDING:
        if (avg_knee > SQUAT_ANGLE_STAND_THRESH)
          {
            uint32_t duration = timestamp_ms - fsm->rep_start_time_ms;
            if (duration >= SQUAT_MIN_REP_DURATION_MS)
              {
                fsm->total_reps++;
                fsm->last_quality_flags = SQUAT_QUALITY_OK;

                /* Quality audit */

                if (fsm->min_knee_angle_curr > SQUAT_ANGLE_SHALLOW_LIMIT)
                  {
                    fsm->last_quality_flags |= SQUAT_QUALITY_SHALLOW;
                    fsm->shallow_count++;
                  }

                if (fsm->min_knee_ankle_ratio_curr <
                    SQUAT_VALGUS_RATIO_LIMIT)
                  {
                    fsm->last_quality_flags |= SQUAT_QUALITY_KNEE_CAVING;
                    fsm->knee_caving_count++;
                  }

                if (fsm->max_trunk_lean_curr > SQUAT_TRUNK_LEAN_LIMIT)
                  {
                    fsm->last_quality_flags |= SQUAT_QUALITY_TRUNK_LEAN;
                    fsm->trunk_lean_count++;
                  }

                if (fsm->last_quality_flags == SQUAT_QUALITY_OK)
                  {
                    fsm->valid_reps++;
                  }

                fsm->last_rep_complete_time_ms = timestamp_ms;
                rep_finished = true;
              }

            fsm->state = SQUAT_STATE_STAND;
            fsm->min_knee_angle_curr = 180.0f;
          }
        break;
    }

  return rep_finished;
}
