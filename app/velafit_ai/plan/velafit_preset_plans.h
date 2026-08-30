/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/plan/velafit_preset_plans.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_PLAN_VELAFIT_PRESET_PLANS_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_PLAN_VELAFIT_PRESET_PLANS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stddef.h>

#include "velafit_plan_scheduler.h"

/****************************************************************************
 * Public Types & Declarations
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

extern const velafit_workout_plan_t g_plan_tabata_core;
extern const velafit_workout_plan_t g_plan_strength_circuit;
extern const velafit_workout_plan_t g_plan_quick_burn;

size_t velafit_get_preset_plan_count(void);

const velafit_workout_plan_t *velafit_get_preset_plan(size_t index);

const velafit_workout_plan_t *velafit_find_preset_plan_by_name(
                                const char *name);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_PLAN_VELAFIT_PRESET_PLANS_H */
