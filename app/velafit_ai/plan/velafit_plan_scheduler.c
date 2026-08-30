/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/plan/velafit_plan_scheduler.c
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "velafit_plan_scheduler.h"
#include "velafit_render.h"
#include "velafit_audio_cue.h"
#include "velafit_storage.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_plan_scheduler_init
 ****************************************************************************/

int velafit_plan_scheduler_init(velafit_plan_scheduler_t *sched,
                                const velafit_workout_plan_t *plan)
{
  if (sched == NULL || plan == NULL || plan->num_steps == 0)
    {
      return -EINVAL;
    }

  memset(sched, 0, sizeof(velafit_plan_scheduler_t));
  memcpy(&sched->plan, plan, sizeof(velafit_workout_plan_t));

  sched->current_step_idx = 0;
  sched->current_phase = VELAFIT_PHASE_PREPARE;
  sched->phase_elapsed_ms = 0;
  sched->phase_target_ms = VELAFIT_PREPARE_DURATION_SEC * 1000;
  sched->total_elapsed_ms = 0;
  sched->total_valid_reps = 0;
  sched->total_calories_kcal = 0.0f;

  /* Initialize pipeline for first exercise */

  const char *first_ex = sched->plan.steps[0].exercise_name;
  int ret = velafit_pipeline_init(&sched->pipeline, first_ex, 320, 240);
  if (ret != OK)
    {
      return ret;
    }

  velafit_audio_cue_play(VELAFIT_AUDIO_CUE_START);
  sched->initialized = true;
  return OK;
}

/****************************************************************************
 * Name: velafit_plan_scheduler_is_finished
 ****************************************************************************/

bool velafit_plan_scheduler_is_finished(
       const velafit_plan_scheduler_t *sched)
{
  if (sched == NULL)
    {
      return true;
    }

  return (sched->current_phase == VELAFIT_PHASE_FINISHED);
}

/****************************************************************************
 * Name: velafit_plan_scheduler_step
 ****************************************************************************/

int velafit_plan_scheduler_step(velafit_plan_scheduler_t *sched,
                                const pose_frame_t *pose,
                                uint32_t dt_ms)
{
  if (sched == NULL || !sched->initialized)
    {
      return -EINVAL;
    }

  sched->total_elapsed_ms += dt_ms;
  sched->phase_elapsed_ms += dt_ms;

  const velafit_workout_step_t *cur_step =
    &sched->plan.steps[sched->current_step_idx];

  switch (sched->current_phase)
    {
      case VELAFIT_PHASE_PREPARE:
        {
          int rem_sec =
            (int)((sched->phase_target_ms - sched->phase_elapsed_ms + 999)
                  / 1000);
          if (rem_sec < 1)
            {
              rem_sec = 1;
            }

          /* Render Prepare Countdown Screen */

          velafit_canvas_clear(&sched->pipeline.canvas,
                               VELAFIT_COLOR_DARKGRAY);

          char header[64];
          snprintf(header, sizeof(header), "PREPARE: STEP %lu/%lu",
                   (unsigned long)(sched->current_step_idx + 1),
                   (unsigned long)sched->plan.num_steps);
          velafit_draw_string(&sched->pipeline.canvas, 20, 30, header,
                              VELAFIT_COLOR_CYAN,
                              VELAFIT_COLOR_DARKGRAY, 2);

          velafit_draw_string(&sched->pipeline.canvas, 20, 70,
                              cur_step->step_desc,
                              VELAFIT_COLOR_WHITE,
                              VELAFIT_COLOR_DARKGRAY, 1);

          char num_str[8];
          snprintf(num_str, sizeof(num_str), "%d", rem_sec);
          velafit_draw_string(&sched->pipeline.canvas, 140, 110, num_str,
                              VELAFIT_COLOR_YELLOW,
                              VELAFIT_COLOR_DARKGRAY, 6);

          velafit_render_to_fb0(&sched->pipeline.canvas);

          if (sched->phase_elapsed_ms >= sched->phase_target_ms)
            {
              sched->current_phase = VELAFIT_PHASE_WORK;
              sched->phase_elapsed_ms = 0;

              if (cur_step->target_type == VELAFIT_TARGET_TIME)
                {
                  sched->phase_target_ms = cur_step->target_value * 1000;
                }
              else
                {
                  sched->phase_target_ms = 120000; /* 2 min max fallback */
                }

              velafit_audio_cue_play(VELAFIT_AUDIO_CUE_WHISTLE);
            }
        }
        break;

      case VELAFIT_PHASE_WORK:
        {
          if (pose != NULL && pose->valid)
            {
              velafit_pipeline_step_pose(&sched->pipeline, pose);
            }

          bool step_completed = false;

          if (cur_step->target_type == VELAFIT_TARGET_TIME)
            {
              if (sched->phase_elapsed_ms >= sched->phase_target_ms)
                {
                  step_completed = true;
                }
            }
          else if (cur_step->target_type == VELAFIT_TARGET_REPS)
            {
              if (sched->pipeline.stats.valid_reps >= cur_step->target_value)
                {
                  step_completed = true;
                }
            }

          if (step_completed)
            {
              sched->total_valid_reps +=
                sched->pipeline.stats.valid_reps;
              sched->total_calories_kcal +=
                sched->pipeline.stats.calories_kcal;

              if (cur_step->rest_duration_sec > 0)
                {
                  sched->current_phase = VELAFIT_PHASE_REST;
                  sched->phase_elapsed_ms = 0;
                  sched->phase_target_ms =
                    cur_step->rest_duration_sec * 1000;
                  velafit_audio_cue_play(VELAFIT_AUDIO_CUE_REST);
                }
              else
                {
                  /* Advance to next step */

                  sched->current_step_idx++;
                  if (sched->current_step_idx < sched->plan.num_steps)
                    {
                      velafit_pipeline_deinit(&sched->pipeline);
                      const char *next_ex =
                        sched->plan.steps[sched->current_step_idx]
                        .exercise_name;
                      velafit_pipeline_init(&sched->pipeline, next_ex,
                                            320, 240);
                      sched->current_phase = VELAFIT_PHASE_PREPARE;
                      sched->phase_elapsed_ms = 0;
                      sched->phase_target_ms =
                        VELAFIT_PREPARE_DURATION_SEC * 1000;
                    }
                  else
                    {
                      sched->current_phase = VELAFIT_PHASE_FINISHED;
                      velafit_audio_cue_play(VELAFIT_AUDIO_CUE_FINISH);
                    }
                }
            }
        }
        break;

      case VELAFIT_PHASE_REST:
        {
          int rem_sec =
            (int)((sched->phase_target_ms - sched->phase_elapsed_ms + 999)
                  / 1000);
          if (rem_sec < 1)
            {
              rem_sec = 1;
            }

          /* Render Rest Overlay Screen */

          velafit_canvas_clear(&sched->pipeline.canvas,
                               VELAFIT_COLOR_DARKGRAY);

          velafit_draw_string(&sched->pipeline.canvas, 20, 30,
                              "REST INTERVAL",
                              VELAFIT_COLOR_GREEN,
                              VELAFIT_COLOR_DARKGRAY, 2);

          char rest_num[16];
          snprintf(rest_num, sizeof(rest_num), "%ds", rem_sec);
          velafit_draw_string(&sched->pipeline.canvas, 120, 90, rest_num,
                              VELAFIT_COLOR_WHITE,
                              VELAFIT_COLOR_DARKGRAY, 5);

          if (sched->current_step_idx + 1 < sched->plan.num_steps)
            {
              char next_msg[64];
              snprintf(next_msg, sizeof(next_msg), "NEXT: %s",
                       sched->plan.steps[sched->current_step_idx + 1]
                       .step_desc);
              velafit_draw_string(&sched->pipeline.canvas, 20, 180,
                                  next_msg, VELAFIT_COLOR_CYAN,
                                  VELAFIT_COLOR_DARKGRAY, 1);
            }

          velafit_render_to_fb0(&sched->pipeline.canvas);

          if (sched->phase_elapsed_ms >= sched->phase_target_ms)
            {
              sched->current_step_idx++;
              if (sched->current_step_idx < sched->plan.num_steps)
                {
                  velafit_pipeline_deinit(&sched->pipeline);
                  const char *next_ex =
                    sched->plan.steps[sched->current_step_idx].exercise_name;
                  velafit_pipeline_init(&sched->pipeline, next_ex, 320, 240);
                  sched->current_phase = VELAFIT_PHASE_PREPARE;
                  sched->phase_elapsed_ms = 0;
                  sched->phase_target_ms =
                    VELAFIT_PREPARE_DURATION_SEC * 1000;
                }
              else
                {
                  sched->current_phase = VELAFIT_PHASE_FINISHED;
                  velafit_audio_cue_play(VELAFIT_AUDIO_CUE_FINISH);
                }
            }
        }
        break;

      case VELAFIT_PHASE_FINISHED:
      default:
        break;
    }

  return OK;
}

/****************************************************************************
 * Name: velafit_plan_scheduler_finish
 ****************************************************************************/

int velafit_plan_scheduler_finish(velafit_plan_scheduler_t *sched,
                                  char *json_buf,
                                  size_t json_max_len)
{
  if (sched == NULL)
    {
      return -EINVAL;
    }

  uint32_t total_sec = sched->total_elapsed_ms / 1000;
  if (total_sec == 0)
    {
      total_sec = 1;
    }

  char sess_id[32];
  snprintf(sess_id, sizeof(sess_id), "VF-PLAN-%04u",
           (unsigned int)(rand() % 10000));

  if (json_buf != NULL && json_max_len > 0)
    {
      snprintf(json_buf, json_max_len,
               "{\n"
               "  \"version\": \"1.0.0\",\n"
               "  \"session_id\": \"%s\",\n"
               "  \"plan_id\": \"%s\",\n"
               "  \"plan_name\": \"%s\",\n"
               "  \"total_duration_sec\": %lu,\n"
               "  \"total_valid_reps\": %lu,\n"
               "  \"total_calories_kcal\": %.2f,\n"
               "  \"completed_steps\": %lu,\n"
               "  \"total_steps\": %lu,\n"
               "  \"status\": \"PLAN_COMPLETED\"\n"
               "}\n",
               sess_id,
               sched->plan.plan_id,
               sched->plan.plan_name,
               (unsigned long)total_sec,
               (unsigned long)sched->total_valid_reps,
               sched->total_calories_kcal,
               (unsigned long)sched->current_step_idx,
               (unsigned long)sched->plan.num_steps);

      /* Persist plan record to storage */

      velafit_session_record_t rec;
      memset(&rec, 0, sizeof(rec));
      strncpy(rec.session_id, sess_id, sizeof(rec.session_id) - 1);
      strncpy(rec.exercise_type, sched->plan.plan_name,
              sizeof(rec.exercise_type) - 1);
      rec.timestamp = 0;
      rec.total_reps = sched->total_valid_reps;
      rec.valid_reps = sched->total_valid_reps;
      rec.duration_sec = total_sec;
      rec.calories_kcal = sched->total_calories_kcal;
      rec.accuracy_pct = 100.0f;
      rec.sync_status = VELAFIT_SYNC_PENDING;

      velafit_storage_save_session(&rec, json_buf);
    }

  return OK;
}

/****************************************************************************
 * Name: velafit_plan_scheduler_deinit
 ****************************************************************************/

void velafit_plan_scheduler_deinit(velafit_plan_scheduler_t *sched)
{
  if (sched != NULL && sched->initialized)
    {
      velafit_pipeline_deinit(&sched->pipeline);
      sched->initialized = false;
    }
}
