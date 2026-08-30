/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/algo/calorie_calc.c
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

#include "calorie_calc.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_calc_calories
 ****************************************************************************/

float velafit_calc_calories(const char *exercise_type,
                            uint32_t duration_sec,
                            float accuracy_pct,
                            float user_weight_kg)
{
  if (user_weight_kg <= 0.0f)
    {
      user_weight_kg = VELAFIT_DEFAULT_USER_WEIGHT_KG;
    }

  float met = 5.0f;

  if (exercise_type != NULL)
    {
      if (strcmp(exercise_type, "SQUAT") == 0 ||
          strcmp(exercise_type, "squat") == 0)
        {
          met = 5.5f;
        }
      else if (strcmp(exercise_type, "JUMPING_JACK") == 0 ||
               strcmp(exercise_type, "jj") == 0)
        {
          met = 8.0f;
        }
      else if (strcmp(exercise_type, "PUSHUP") == 0 ||
               strcmp(exercise_type, "pushup") == 0)
        {
          met = 8.0f;
        }
      else if (strcmp(exercise_type, "PLANK") == 0 ||
               strcmp(exercise_type, "plank") == 0)
        {
          met = 3.8f;
        }
    }

  float hours = (float)duration_sec / 3600.0f;
  float quality_multiplier = 0.5f + 0.5f * (accuracy_pct / 100.0f);

  if (quality_multiplier < 0.5f)
    {
      quality_multiplier = 0.5f;
    }

  if (quality_multiplier > 1.0f)
    {
      quality_multiplier = 1.0f;
    }

  return met * user_weight_kg * hours * quality_multiplier;
}
