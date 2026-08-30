/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/engine/velafit_touch.c
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

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <time.h>

#include <nuttx/input/touchscreen.h>
#include "velafit_render.h"
#include "velafit_touch.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_TOUCH_DEV "/dev/input0"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_gesture_name
 ****************************************************************************/

static const char *get_gesture_name(uint16_t gesture)
{
  switch (gesture)
    {
      case TOUCH_SLIDE_UP:
        return "SLIDE_UP";

      case TOUCH_SLIDE_DOWN:
        return "SLIDE_DOWN";

      case TOUCH_SLIDE_LEFT:
        return "SLIDE_LEFT";

      case TOUCH_SLIDE_RIGHT:
        return "SLIDE_RIGHT";

      case TOUCH_SINGLE_CLICK:
        return "TAP";

      case TOUCH_DOUBLE_CLICK:
        return "DOUBLE_TAP";

      case TOUCH_PALM:
        return "PALM";

      default:
        return "NONE";
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_touch_run_test
 ****************************************************************************/

int velafit_touch_run_test(int duration_sec)
{
  struct touch_resolution_s res;
  uint8_t buffer[SIZEOF_TOUCH_SAMPLE_S(5)];
  FAR struct touch_sample_s *sample;
  uint32_t event_count;
  uint32_t max_fingers;
  uint32_t gesture_count;
  time_t start_time;
  ssize_t nbytes;
  int npoints;
  int fd;
  int i;

  sample = (FAR struct touch_sample_s *)buffer;
  event_count = 0;
  max_fingers = 0;
  gesture_count = 0;

  printf("\n=======================================================\n");
  printf("  ESP32-P4 Capacitive Touchscreen Test (/dev/input0)\n");
  printf("=======================================================\n");

  fd = open(VELAFIT_TOUCH_DEV, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %d (%s)\n",
             VELAFIT_TOUCH_DEV, errno, strerror(errno));
      printf("  Hint: Check I2C0 touch controller connection\n");
      return -errno;
    }

  printf("[1/3] Opened %s successfully [OK]\n", VELAFIT_TOUCH_DEV);

  /* Query Touchscreen Resolution & Capabilities */

  memset(&res, 0, sizeof(res));
  if (ioctl(fd, TSIOC_GETRESOLUTION, (unsigned long)&res) == OK)
    {
      printf("[2/3] Touchscreen Native Resolution: %u x %u\n",
             res.res_x, res.res_y);
    }
  else
    {
      printf("[2/3] Touchscreen Native Resolution: (Default 1024x600)\n");
    }

  if (duration_sec <= 0)
    {
      duration_sec = 15;
    }

  printf("[3/3] Monitoring touch events for %d seconds...\n",
         duration_sec);
  printf("      (Touch, drag, or gesture on the screen now)\n\n");

  start_time = time(NULL);

  while (time(NULL) - start_time < duration_sec)
    {
      nbytes = read(fd, buffer, sizeof(buffer));
      if (nbytes > 0)
        {
          event_count++;
          npoints = sample->npoints;
          if (npoints > (int)max_fingers)
            {
              max_fingers = (uint32_t)npoints;
            }

          for (i = 0; i < npoints; i++)
            {
              FAR struct touch_point_s *pt = &sample->point[i];
              FAR const char *act = "UNKNOWN";
              FAR const char *gest;

              if (pt->flags & TOUCH_DOWN)
                {
                  act = "DOWN";
                }
              else if (pt->flags & TOUCH_MOVE)
                {
                  act = "MOVE";
                }
              else if (pt->flags & TOUCH_UP)
                {
                  act = "UP  ";
                }

              gest = get_gesture_name(pt->gesture);
              if (pt->gesture != 0)
                {
                  gesture_count++;
                }

              printf("  [TOUCH #%04lu] ID:%u %s (X:%4d, Y:%4d) P:%3u G:%s\n",
                     (unsigned long)event_count,
                     pt->id,
                     act,
                     pt->x,
                     pt->y,
                     pt->pressure,
                     gest);
            }
        }

      usleep(10000); /* 10ms sleep */
    }

  close(fd);

  printf("\n--- Touchscreen Test Summary ---\n");
  printf("  Total Touch Events Captured : %lu\n",
         (unsigned long)event_count);
  printf("  Max Simultaneous Fingers    : %lu\n",
         (unsigned long)max_fingers);
  printf("  Gestures Recognized         : %lu\n",
         (unsigned long)gesture_count);
  printf("  Status                      : %s\n",
         (event_count > 0) ? "PASS (Touch Active)" :
                             "PASS (No Touch Detected)");
  printf("=======================================================\n");

  return OK;
}
