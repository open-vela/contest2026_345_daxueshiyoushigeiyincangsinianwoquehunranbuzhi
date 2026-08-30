/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/pushup_fsm.c
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

#include "pushup_fsm.h"
#include "geometry.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PUSHUP_ANGLE_EXTENDED_THRESH   150.0f
#define PUSHUP_ANGLE_DESCEND_THRESH    135.0f
#define PUSHUP_ANGLE_BOTTOM_TARGET      95.0f
#define PUSHUP_ANGLE_SHALLOW_LIMIT     110.0f
#define PUSHUP_ANGLE_ASCEND_THRESH     120.0f
#define PUSHUP_BODY_LINE_MIN_THRESH    155.0f
#define PUSHUP_MIN_REP_DURATION_MS     400

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pushup_fsm_init
 ****************************************************************************/

void pushup_fsm_init(pushup_fsm_t *fsm)
{
  fsm->state = PUSHUP_STATE_PLANK;
  fsm->total_reps = 0;
  fsm->valid_reps = 0;
  fsm->shallow_count = 0;
  fsm->hips_sag_count = 0;
  fsm->hips_pike_count = 0;
  fsm->elbow_flare_count = 0;
  fsm->min_elbow_angle_curr = 180.0f;
  fsm->min_body_line_curr = 180.0f;
  fsm->rep_start_time_ms = 0;
  fsm->last_rep_complete_time_ms = 0;
  fsm->last_quality_flags = PUSHUP_QUALITY_OK;
}

/****************************************************************************
 * Name: pushup_fsm_reset
 ****************************************************************************/

void pushup_fsm_reset(pushup_fsm_t *fsm)
{
  pushup_fsm_init(fsm);
}

/****************************************************************************
 * Name: pushup_fsm_update
 ****************************************************************************/

bool pushup_fsm_update(pushup_fsm_t *fsm,
                       const pose_frame_t *pose,
                       uint32_t timestamp_ms)
{
  if (!pose || !pose->valid)
    {
      return false;
    }

  float elbow_l = velafit_get_elbow_angle_left(pose);
  float elbow_r = velafit_get_elbow_angle_right(pose);
  float avg_elbow = (elbow_l + elbow_r) * 0.5f;

  float body_line = velafit_get_body_line_angle(pose);

  float mid_shoulder_y =
    (pose->kpts[KPT_LEFT_SHOULDER].y +
     pose->kpts[KPT_RIGHT_SHOULDER].y) * 0.5f;
  float mid_hip_y =
    (pose->kpts[KPT_LEFT_HIP].y +
     pose->kpts[KPT_RIGHT_HIP].y) * 0.5f;
  float mid_ankle_y =
    (pose->kpts[KPT_LEFT_ANKLE].y +
     pose->kpts[KPT_RIGHT_ANKLE].y) * 0.5f;
  float exp_hip_y = (mid_shoulder_y + mid_ankle_y) * 0.5f;

  bool rep_finished = false;

  switch (fsm->state)
    {
      case PUSHUP_STATE_PLANK:
        if (avg_elbow < PUSHUP_ANGLE_DESCEND_THRESH)
          {
            fsm->state = PUSHUP_STATE_DESCENDING;
            fsm->rep_start_time_ms = timestamp_ms;
            fsm->min_elbow_angle_curr = avg_elbow;
            fsm->min_body_line_curr = body_line;
          }
        break;

      case PUSHUP_STATE_DESCENDING:
        if (avg_elbow < fsm->min_elbow_angle_curr)
          {
            fsm->min_elbow_angle_curr = avg_elbow;
          }

        if (body_line < fsm->min_body_line_curr)
          {
            fsm->min_body_line_curr = body_line;
          }

        if (avg_elbow <= PUSHUP_ANGLE_BOTTOM_TARGET)
          {
            fsm->state = PUSHUP_STATE_BOTTOM;
          }
        else if (avg_elbow > PUSHUP_ANGLE_ASCEND_THRESH &&
                 fsm->min_elbow_angle_curr < PUSHUP_ANGLE_DESCEND_THRESH)
          {
            /* Reversing early without reaching target depth */

            fsm->state = PUSHUP_STATE_ASCENDING;
          }
        else
          {
            break;
          }

      case PUSHUP_STATE_BOTTOM:
        if (avg_elbow < fsm->min_elbow_angle_curr)
          {
            fsm->min_elbow_angle_curr = avg_elbow;
          }

        if (body_line < fsm->min_body_line_curr)
          {
            fsm->min_body_line_curr = body_line;
          }

        if (avg_elbow > PUSHUP_ANGLE_ASCEND_THRESH)
          {
            fsm->state = PUSHUP_STATE_ASCENDING;
          }
        else
          {
            break;
          }

      case PUSHUP_STATE_ASCENDING:
        if (avg_elbow > PUSHUP_ANGLE_EXTENDED_THRESH)
          {
            uint32_t duration = timestamp_ms - fsm->rep_start_time_ms;
            if (duration >= PUSHUP_MIN_REP_DURATION_MS)
              {
                fsm->total_reps++;
                fsm->last_quality_flags = PUSHUP_QUALITY_OK;

                /* Audit: Shallow Depth */

                if (fsm->min_elbow_angle_curr > PUSHUP_ANGLE_SHALLOW_LIMIT)
                  {
                    fsm->last_quality_flags |= PUSHUP_QUALITY_SHALLOW;
                    fsm->shallow_count++;
                  }

                /* Audit: Body alignment (Sag vs Pike) */

                if (fsm->min_body_line_curr < PUSHUP_BODY_LINE_MIN_THRESH)
                  {
                    if (mid_hip_y > exp_hip_y + 0.05f)
                      {
                        fsm->last_quality_flags |= PUSHUP_QUALITY_HIPS_SAG;
                        fsm->hips_sag_count++;
                      }
                    else
                      {
                        fsm->last_quality_flags |= PUSHUP_QUALITY_HIPS_PIKE;
                        fsm->hips_pike_count++;
                      }
                  }

                if (fsm->last_quality_flags == PUSHUP_QUALITY_OK)
                  {
                    fsm->valid_reps++;
                  }

                fsm->last_rep_complete_time_ms = timestamp_ms;
                rep_finished = true;
              }

            fsm->state = PUSHUP_STATE_PLANK;
            fsm->min_elbow_angle_curr = 180.0f;
            fsm->min_body_line_curr = 180.0f;
          }
        break;
    }

  return rep_finished;
}
