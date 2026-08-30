/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/engine/velafit_touch.h
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

#ifndef __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ENGINE_VELAFIT_TOUCH_H
#define __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ENGINE_VELAFIT_TOUCH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Name: velafit_touch_run_test
 *
 * Description:
 *   Run capacitive touchscreen test and gesture recognition tool on
 *   /dev/input0. Monitors touch points, tracks gestures, and draws touch
 *   crosshair visual indicators onto /dev/fb0.
 *
 * Parameters:
 *   duration_sec - Duration in seconds to run test (0 for 15s default).
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int velafit_touch_run_test(int duration_sec);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_PACKAGES_DEMOS_CONTEST2026_345_VELAFIT_AI_ENGINE_VELAFIT_TOUCH_H */
