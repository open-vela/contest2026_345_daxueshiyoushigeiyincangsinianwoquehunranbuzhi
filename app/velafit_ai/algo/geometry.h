/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/geometry.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_GEOMETRY_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_GEOMETRY_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "velafit_types.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

float velafit_calc_angle_3p(kpt_2d_t a, kpt_2d_t b, kpt_2d_t c);

float velafit_calc_dist_2p(kpt_2d_t a, kpt_2d_t b);

float velafit_get_knee_angle_left(const pose_frame_t *pose);

float velafit_get_knee_angle_right(const pose_frame_t *pose);

float velafit_get_hip_angle_left(const pose_frame_t *pose);

float velafit_get_hip_angle_right(const pose_frame_t *pose);

float velafit_get_elbow_angle_left(const pose_frame_t *pose);

float velafit_get_elbow_angle_right(const pose_frame_t *pose);

float velafit_get_body_line_angle(const pose_frame_t *pose);

float velafit_get_leg_spread_ratio(const pose_frame_t *pose);

float velafit_get_knee_to_ankle_ratio(const pose_frame_t *pose);

float velafit_get_trunk_lean_angle(const pose_frame_t *pose);

float velafit_get_arm_angle_left(const pose_frame_t *pose);

float velafit_get_arm_angle_right(const pose_frame_t *pose);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ALGO_GEOMETRY_H */
