/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/plan/velafit_preset_plans.c
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

#include <string.h>
#include <strings.h>

#include "velafit_preset_plans.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Preset 1: Tabata 4-Minute Full Body HIIT */

const velafit_workout_plan_t g_plan_tabata_core =
{
  "PLAN-TABATA-001",
  "tabata",
  "4-Minute Full-Body Tabata HIIT Routine",
  4,
  {
    {
      "jj",
      VELAFIT_TARGET_TIME,
      20,
      10,
      "Round 1: Jumping Jacks"
    },
    {
      "squat",
      VELAFIT_TARGET_TIME,
      20,
      10,
      "Round 2: Deep Squats"
    },
    {
      "pushup",
      VELAFIT_TARGET_TIME,
      20,
      10,
      "Round 3: Push-ups"
    },
    {
      "plank",
      VELAFIT_TARGET_TIME,
      20,
      0,
      "Round 4: Core Plank Hold"
    }
  }
};

/* Preset 2: Strength Circuit (Target Reps) */

const velafit_workout_plan_t g_plan_strength_circuit =
{
  "PLAN-STRENGTH-002",
  "strength",
  "Target-Rep Strength Circuit",
  3,
  {
    {
      "squat",
      VELAFIT_TARGET_REPS,
      10,
      15,
      "Set 1: 10 Target Squats"
    },
    {
      "pushup",
      VELAFIT_TARGET_REPS,
      10,
      15,
      "Set 2: 10 Push-ups"
    },
    {
      "plank",
      VELAFIT_TARGET_TIME,
      20,
      0,
      "Set 3: 20s Core Plank"
    }
  }
};

/* Preset 3: Quick Cardio Burn */

const velafit_workout_plan_t g_plan_quick_burn =
{
  "PLAN-CARDIO-003",
  "cardio",
  "Quick 1-Minute Cardio Burn",
  2,
  {
    {
      "jj",
      VELAFIT_TARGET_TIME,
      30,
      10,
      "Interval 1: 30s Jumping Jacks"
    },
    {
      "squat",
      VELAFIT_TARGET_TIME,
      30,
      0,
      "Interval 2: 30s Squats"
    }
  }
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const velafit_workout_plan_t * const g_preset_plans[] =
{
  &g_plan_tabata_core,
  &g_plan_strength_circuit,
  &g_plan_quick_burn
};

#define PRESET_PLAN_COUNT (sizeof(g_preset_plans) / sizeof(g_preset_plans[0]))

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_get_preset_plan_count
 ****************************************************************************/

size_t velafit_get_preset_plan_count(void)
{
  return PRESET_PLAN_COUNT;
}

/****************************************************************************
 * Name: velafit_get_preset_plan
 ****************************************************************************/

const velafit_workout_plan_t *velafit_get_preset_plan(size_t index)
{
  if (index >= PRESET_PLAN_COUNT)
    {
      return NULL;
    }

  return g_preset_plans[index];
}

/****************************************************************************
 * Name: velafit_find_preset_plan_by_name
 ****************************************************************************/

const velafit_workout_plan_t *velafit_find_preset_plan_by_name(
                                const char *name)
{
  if (name == NULL)
    {
      return NULL;
    }

  for (size_t i = 0; i < PRESET_PLAN_COUNT; i++)
    {
      if (strcasecmp(g_preset_plans[i]->plan_name, name) == 0 ||
          strcasecmp(g_preset_plans[i]->plan_id, name) == 0)
        {
          return g_preset_plans[i];
        }
    }

  return NULL;
}
