/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/geometry.c
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

#include <math.h>

#include "geometry.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_calc_dist_2p
 ****************************************************************************/

float velafit_calc_dist_2p(kpt_2d_t a, kpt_2d_t b)
{
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return sqrtf(dx * dx + dy * dy);
}

/****************************************************************************
 * Name: velafit_calc_angle_3p
 ****************************************************************************/

float velafit_calc_angle_3p(kpt_2d_t a, kpt_2d_t b, kpt_2d_t c)
{
  float v1x = a.x - b.x;
  float v1y = a.y - b.y;
  float v2x = c.x - b.x;
  float v2y = c.y - b.y;

  float dot = v1x * v2x + v1y * v2y;
  float mag1 = sqrtf(v1x * v1x + v1y * v1y);
  float mag2 = sqrtf(v2x * v2x + v2y * v2y);

  if (mag1 < 1e-5f || mag2 < 1e-5f)
    {
      return 180.0f;
    }

  float cos_angle = dot / (mag1 * mag2);
  if (cos_angle > 1.0f)
    {
      cos_angle = 1.0f;
    }

  if (cos_angle < -1.0f)
    {
      cos_angle = -1.0f;
    }

  return acosf(cos_angle) * (180.0f / (float)M_PI);
}

/****************************************************************************
 * Name: velafit_get_knee_angle_left
 ****************************************************************************/

float velafit_get_knee_angle_left(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_LEFT_HIP],
                               pose->kpts[KPT_LEFT_KNEE],
                               pose->kpts[KPT_LEFT_ANKLE]);
}

/****************************************************************************
 * Name: velafit_get_knee_angle_right
 ****************************************************************************/

float velafit_get_knee_angle_right(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_RIGHT_HIP],
                               pose->kpts[KPT_RIGHT_KNEE],
                               pose->kpts[KPT_RIGHT_ANKLE]);
}

/****************************************************************************
 * Name: velafit_get_hip_angle_left
 ****************************************************************************/

float velafit_get_hip_angle_left(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_LEFT_SHOULDER],
                               pose->kpts[KPT_LEFT_HIP],
                               pose->kpts[KPT_LEFT_KNEE]);
}

/****************************************************************************
 * Name: velafit_get_hip_angle_right
 ****************************************************************************/

float velafit_get_hip_angle_right(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_RIGHT_SHOULDER],
                               pose->kpts[KPT_RIGHT_HIP],
                               pose->kpts[KPT_RIGHT_KNEE]);
}

/****************************************************************************
 * Name: velafit_get_elbow_angle_left
 ****************************************************************************/

float velafit_get_elbow_angle_left(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_LEFT_WRIST],
                               pose->kpts[KPT_LEFT_ELBOW],
                               pose->kpts[KPT_LEFT_SHOULDER]);
}

/****************************************************************************
 * Name: velafit_get_elbow_angle_right
 ****************************************************************************/

float velafit_get_elbow_angle_right(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_RIGHT_WRIST],
                               pose->kpts[KPT_RIGHT_ELBOW],
                               pose->kpts[KPT_RIGHT_SHOULDER]);
}

/****************************************************************************
 * Name: velafit_get_body_line_angle
 ****************************************************************************/

float velafit_get_body_line_angle(const pose_frame_t *pose)
{
  kpt_2d_t mid_shoulder;
  mid_shoulder.x = (pose->kpts[KPT_LEFT_SHOULDER].x +
                    pose->kpts[KPT_RIGHT_SHOULDER].x) * 0.5f;
  mid_shoulder.y = (pose->kpts[KPT_LEFT_SHOULDER].y +
                    pose->kpts[KPT_RIGHT_SHOULDER].y) * 0.5f;
  mid_shoulder.score = 1.0f;

  kpt_2d_t mid_hip;
  mid_hip.x = (pose->kpts[KPT_LEFT_HIP].x +
               pose->kpts[KPT_RIGHT_HIP].x) * 0.5f;
  mid_hip.y = (pose->kpts[KPT_LEFT_HIP].y +
               pose->kpts[KPT_RIGHT_HIP].y) * 0.5f;
  mid_hip.score = 1.0f;

  kpt_2d_t mid_ankle;
  mid_ankle.x = (pose->kpts[KPT_LEFT_ANKLE].x +
                 pose->kpts[KPT_RIGHT_ANKLE].x) * 0.5f;
  mid_ankle.y = (pose->kpts[KPT_LEFT_ANKLE].y +
                 pose->kpts[KPT_RIGHT_ANKLE].y) * 0.5f;
  mid_ankle.score = 1.0f;

  return velafit_calc_angle_3p(mid_shoulder, mid_hip, mid_ankle);
}

/****************************************************************************
 * Name: velafit_get_leg_spread_ratio
 ****************************************************************************/

float velafit_get_leg_spread_ratio(const pose_frame_t *pose)
{
  float d_ankles = velafit_calc_dist_2p(pose->kpts[KPT_LEFT_ANKLE],
                                        pose->kpts[KPT_RIGHT_ANKLE]);
  float d_shoulders = velafit_calc_dist_2p(pose->kpts[KPT_LEFT_SHOULDER],
                                           pose->kpts[KPT_RIGHT_SHOULDER]);
  if (d_shoulders < 1e-4f)
    {
      return 1.0f;
    }

  return d_ankles / d_shoulders;
}

/****************************************************************************
 * Name: velafit_get_knee_to_ankle_ratio
 ****************************************************************************/

float velafit_get_knee_to_ankle_ratio(const pose_frame_t *pose)
{
  float d_knees = velafit_calc_dist_2p(pose->kpts[KPT_LEFT_KNEE],
                                       pose->kpts[KPT_RIGHT_KNEE]);
  float d_ankles = velafit_calc_dist_2p(pose->kpts[KPT_LEFT_ANKLE],
                                        pose->kpts[KPT_RIGHT_ANKLE]);
  if (d_ankles < 1e-4f)
    {
      return 1.0f;
    }

  return d_knees / d_ankles;
}

/****************************************************************************
 * Name: velafit_get_trunk_lean_angle
 ****************************************************************************/

float velafit_get_trunk_lean_angle(const pose_frame_t *pose)
{
  kpt_2d_t mid_shoulder;
  mid_shoulder.x = (pose->kpts[KPT_LEFT_SHOULDER].x +
                    pose->kpts[KPT_RIGHT_SHOULDER].x) * 0.5f;
  mid_shoulder.y = (pose->kpts[KPT_LEFT_SHOULDER].y +
                    pose->kpts[KPT_RIGHT_SHOULDER].y) * 0.5f;
  mid_shoulder.score = 1.0f;

  kpt_2d_t mid_hip;
  mid_hip.x = (pose->kpts[KPT_LEFT_HIP].x +
               pose->kpts[KPT_RIGHT_HIP].x) * 0.5f;
  mid_hip.y = (pose->kpts[KPT_LEFT_HIP].y +
               pose->kpts[KPT_RIGHT_HIP].y) * 0.5f;
  mid_hip.score = 1.0f;

  kpt_2d_t ref_vertical;
  ref_vertical.x = mid_hip.x;
  ref_vertical.y = mid_hip.y - 0.5f;
  ref_vertical.score = 1.0f;

  return velafit_calc_angle_3p(mid_shoulder, mid_hip, ref_vertical);
}

/****************************************************************************
 * Name: velafit_get_arm_angle_left
 ****************************************************************************/

float velafit_get_arm_angle_left(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_LEFT_WRIST],
                               pose->kpts[KPT_LEFT_SHOULDER],
                               pose->kpts[KPT_LEFT_HIP]);
}

/****************************************************************************
 * Name: velafit_get_arm_angle_right
 ****************************************************************************/

float velafit_get_arm_angle_right(const pose_frame_t *pose)
{
  return velafit_calc_angle_3p(pose->kpts[KPT_RIGHT_WRIST],
                               pose->kpts[KPT_RIGHT_SHOULDER],
                               pose->kpts[KPT_RIGHT_HIP]);
}
