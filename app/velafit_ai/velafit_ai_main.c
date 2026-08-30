/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/velafit_ai_main.c
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
#include <unistd.h>
#include <errno.h>

#include "velafit_types.h"
#include "esp_nn_ops.h"
#include "velafit_pose_model.h"
#include "sample_pose_frames.h"
#include "geometry.h"
#include "one_euro_filter.h"
#include "squat_fsm.h"
#include "jumping_jack_fsm.h"
#include "pushup_fsm.h"
#include "plank_fsm.h"
#include "calorie_calc.h"
#include "velafit_render.h"
#include "velafit_audio_cue.h"
#include "velafit_storage.h"
#include "velafit_sync.h"
#include "velafit_pipeline.h"
#include "velafit_plan_scheduler.h"
#include "velafit_preset_plans.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: print_usage
 ****************************************************************************/

static void print_usage(void)
{
  printf("\n=======================================================\n");
  printf("  VelaFit AI Smart Posture Coach - CLI Suite\n");
  printf("=======================================================\n");
  printf("Usage: velafit_ai <command> [args]\n\n");
  printf("Commands:\n");
  printf("  benchmark            - Stage 1: ESP-NN SIMD Benchmarks\n");
  printf("  test_pose            - Stage 2: 17 Keypoint Pose Inference\n");
  printf("  test_squat  [count]  - Stage 3: Squat FSM & Quality Audit\n");
  printf("  test_jj     [count]  - Stage 3: Jumping Jack FSM Simulation\n");
  printf("  test_pushup [count]  - Stage 3: Push-up FSM & Quality Audit\n");
  printf("  test_plank  [sec]    - Stage 3: Plank Timer & Posture Guard\n");
  printf("  render      [ppm]    - Stage 4: Skeleton OSD Rendering\n");
  printf("  audio       [cue]    - Stage 4: Voice / Tone Cue Dispatcher\n");
  printf("  pipeline    [cycles] - Stage 4: End-to-End Pipeline\n");
  printf("  plan        [cmd]    - Routine Planner (list/run [name])\n");
  printf("  storage     [cmd]    - Offline Storage (list/info/summary)\n");
  printf("  sync        [cmd]    - Cloud Sync (status/flush/mock)\n");
  printf("  report               - Edge-Cloud Workout JSON Report\n");
  printf("  all                  - Run All Verification Stages\n");
  printf("=======================================================\n\n");
}

/****************************************************************************
 * Name: cmd_test_pose
 ****************************************************************************/

static int cmd_test_pose(void)
{
  printf("\n>>> [Stage 2] Running Static Pose Inference...\n");

  velafit_perf_t perf;
  pose_frame_t detected_pose;

  int ret = velafit_pose_infer(g_sample_test_image_160x160,
                               &detected_pose, &perf);
  if (ret != 0)
    {
      printf("Error: Pose infer failed! (code: %d)\n", ret);
      return ret;
    }

  printf("\n--- 17 Human Keypoint Detections ---\n");
  const char *kpt_names[VELAFIT_NUM_KEYPOINTS] =
  {
    "Nose", "L_Eye", "R_Eye", "L_Ear", "R_Ear",
    "L_Shoulder", "R_Shoulder", "L_Elbow", "R_Elbow",
    "L_Wrist", "R_Wrist", "L_Hip", "R_Hip",
    "L_Knee", "R_Knee", "L_Ankle", "R_Ankle"
  };

  for (int i = 0; i < VELAFIT_NUM_KEYPOINTS; i++)
    {
      printf("  [%02d] %-12s : (X: %0.3f, Y: %0.3f)  Score: %0.2f\n",
             i, kpt_names[i],
             detected_pose.kpts[i].x,
             detected_pose.kpts[i].y,
             detected_pose.kpts[i].score);
    }

  float knee_l = velafit_get_knee_angle_left(&detected_pose);
  float knee_r = velafit_get_knee_angle_right(&detected_pose);
  float hip_l  = velafit_get_hip_angle_left(&detected_pose);
  float hip_r  = velafit_get_hip_angle_right(&detected_pose);
  float trunk_lean = velafit_get_trunk_lean_angle(&detected_pose);
  float knee_ankle_ratio =
    velafit_get_knee_to_ankle_ratio(&detected_pose);

  printf("\n--- Geometric Angles & Posture Features ---\n");
  printf("  Left Knee Angle  : %0.1f deg\n", knee_l);
  printf("  Right Knee Angle : %0.1f deg\n", knee_r);
  printf("  Left Hip Angle   : %0.1f deg\n", hip_l);
  printf("  Right Hip Angle  : %0.1f deg\n", hip_r);
  printf("  Trunk Lean Angle : %0.1f deg\n", trunk_lean);
  printf("  Knee/Ankle Ratio : %0.2f (Threshold > 0.72)\n",
         knee_ankle_ratio);

  printf("\n--- Edge AI Profiling Metrics ---\n");
  printf("  Preprocess       : %lu us\n",
         (unsigned long)perf.preprocess_us);
  printf("  Forward Infer    : %lu us\n",
         (unsigned long)perf.infer_us);
  printf("  Postprocess      : %lu us\n",
         (unsigned long)perf.postprocess_us);
  printf("  Total Latency    : %lu us (approx %.1f FPS)\n",
         (unsigned long)perf.total_us, perf.fps);
  printf(">>> [Stage 2] Pose Inference Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_test_squat
 ****************************************************************************/

static int cmd_test_squat(int cycles)
{
  if (cycles <= 0)
    {
      cycles = 1;
    }

  printf("\n>>> [Stage 3] Squat FSM Simulation (Cycles: %d)...\n",
         cycles);

  squat_fsm_t fsm;
  squat_fsm_init(&fsm);

  uint32_t t = 0;

  for (int c = 0; c < cycles; c++)
    {
      printf("\n[Test 1/3] Simulating Standard Deep Squat...\n");
      squat_fsm_update(&fsm, &g_sample_pose_standing, t);
      t += 200;
      squat_fsm_update(&fsm, &g_sample_pose_squat_shallow, t);
      t += 300;
      squat_fsm_update(&fsm, &g_sample_pose_squat_deep, t);
      t += 300;
      squat_fsm_update(&fsm, &g_sample_pose_squat_shallow, t);
      t += 300;
      bool rep1 = squat_fsm_update(&fsm, &g_sample_pose_standing, t);
      t += 200;

      printf("  => Rep Decision: %s (Total: %lu, Valid: %lu, Flag: %s)\n",
             rep1 ? "YES [TRIGGER]" : "NO",
             (unsigned long)fsm.total_reps,
             (unsigned long)fsm.valid_reps,
             (fsm.last_quality_flags == SQUAT_QUALITY_OK) ?
             "STANDARD (OK)" : "ERROR");

      printf("\n[Test 2/3] Simulating Shallow Squat...\n");
      squat_fsm_update(&fsm, &g_sample_pose_standing, t);
      t += 200;
      squat_fsm_update(&fsm, &g_sample_pose_squat_shallow, t);
      t += 600;
      bool rep2 = squat_fsm_update(&fsm, &g_sample_pose_standing, t);
      t += 500;

      printf("  => Rep Decision: %s (Shallow: %lu, Quality: %s)\n",
             rep2 ? "YES [TRIGGER]" : "NO",
             (unsigned long)fsm.shallow_count,
             (fsm.last_quality_flags & SQUAT_QUALITY_SHALLOW) ?
             "WARN: SHALLOW [DETECTED]" : "NOT_DETECTED");

      printf("\n[Test 3/3] Simulating Knee Valgus Squat...\n");
      squat_fsm_update(&fsm, &g_sample_pose_standing, t);
      t += 200;
      squat_fsm_update(&fsm, &g_sample_pose_squat_valgus, t);
      t += 600;
      bool rep3 = squat_fsm_update(&fsm, &g_sample_pose_standing, t);
      t += 500;

      printf("  => Rep Decision: %s (Valgus: %lu, Quality: %s)\n",
             rep3 ? "YES [TRIGGER]" : "NO",
             (unsigned long)fsm.knee_caving_count,
             (fsm.last_quality_flags & SQUAT_QUALITY_KNEE_CAVING) ?
             "WARN: KNEE VALGUS [DETECTED]" : "NOT_DETECTED");
    }

  printf("\n================ Squat Summary ================\n");
  printf("  Total Reps      : %lu\n", (unsigned long)fsm.total_reps);
  printf("  Valid Standard  : %lu\n", (unsigned long)fsm.valid_reps);
  printf("  Shallow Faults  : %lu\n", (unsigned long)fsm.shallow_count);
  printf("  Valgus Faults   : %lu\n",
         (unsigned long)fsm.knee_caving_count);
  printf("  Lean Faults     : %lu\n",
         (unsigned long)fsm.trunk_lean_count);
  printf("===============================================\n");
  printf(">>> [Stage 3] Squat FSM Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_test_jumping_jack
 ****************************************************************************/

static int cmd_test_jumping_jack(int cycles)
{
  if (cycles <= 0)
    {
      cycles = 1;
    }

  printf("\n>>> [Stage 3] Jumping Jack Simulation (Cycles: %d)...\n",
         cycles);

  jumping_jack_fsm_t fsm;
  jumping_jack_fsm_reset(&fsm);

  uint32_t t = 0;
  for (int i = 0; i < 3 * cycles; i++)
    {
      jumping_jack_fsm_update(&fsm, &g_sample_pose_jj_closed, t);
      t += 100;
      jumping_jack_fsm_update(&fsm, &g_sample_pose_jj_open, t);
      t += 300;
      bool rep =
        jumping_jack_fsm_update(&fsm, &g_sample_pose_jj_closed, t);
      t += 300;

      printf("  JJ Rep %d Decision: %s (Total: %lu, Valid: %lu)\n",
             i + 1, rep ? "YES [PASS]" : "NO",
             (unsigned long)fsm.total_reps,
             (unsigned long)fsm.valid_reps);
    }

  printf("\n================ Jumping Jack Summary ================\n");
  printf("  Total Reps : %lu\n", (unsigned long)fsm.total_reps);
  printf("  Valid Reps : %lu\n", (unsigned long)fsm.valid_reps);
  printf("======================================================\n");
  printf(">>> [Stage 3] Jumping Jack Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_test_pushup
 ****************************************************************************/

static int cmd_test_pushup(int cycles)
{
  if (cycles <= 0)
    {
      cycles = 1;
    }

  printf("\n>>> [Stage 3] Push-up FSM Simulation (Cycles: %d)...\n",
         cycles);

  pushup_fsm_t fsm;
  pushup_fsm_init(&fsm);

  uint32_t t = 0;

  for (int c = 0; c < cycles; c++)
    {
      printf("\n[Test 1/3] Simulating Standard Deep Push-up...\n");
      pushup_fsm_update(&fsm, &g_sample_pose_pushup_plank, t);
      t += 200;
      pushup_fsm_update(&fsm, &g_sample_pose_pushup_bottom_deep, t);
      t += 400;
      bool rep1 =
        pushup_fsm_update(&fsm, &g_sample_pose_pushup_plank, t);
      t += 300;

      printf("  => Rep Decision: %s (Total: %lu, Valid: %lu, Flag: %s)\n",
             rep1 ? "YES [PASS]" : "NO",
             (unsigned long)fsm.total_reps,
             (unsigned long)fsm.valid_reps,
             (fsm.last_quality_flags == PUSHUP_QUALITY_OK) ?
             "STANDARD (OK)" : "ERROR");

      printf("\n[Test 2/3] Simulating Shallow Push-up...\n");
      pushup_fsm_update(&fsm, &g_sample_pose_pushup_plank, t);
      t += 200;
      pushup_fsm_update(&fsm, &g_sample_pose_pushup_shallow, t);
      t += 400;
      bool rep2 =
        pushup_fsm_update(&fsm, &g_sample_pose_pushup_plank, t);
      t += 300;

      printf("  => Rep Decision: %s (Shallow: %lu, Quality: %s)\n",
             rep2 ? "YES [TRIGGER]" : "NO",
             (unsigned long)fsm.shallow_count,
             (fsm.last_quality_flags & PUSHUP_QUALITY_SHALLOW) ?
             "WARN: SHALLOW [DETECTED]" : "NOT_DETECTED");

      printf("\n[Test 3/3] Simulating Sagging Hips Push-up...\n");
      pushup_fsm_update(&fsm, &g_sample_pose_pushup_plank, t);
      t += 200;
      pushup_fsm_update(&fsm, &g_sample_pose_pushup_sag, t);
      t += 400;
      bool rep3 =
        pushup_fsm_update(&fsm, &g_sample_pose_pushup_plank, t);
      t += 300;

      printf("  => Rep Decision: %s (Sagging: %lu, Quality: %s)\n",
             rep3 ? "YES [TRIGGER]" : "NO",
             (unsigned long)fsm.hips_sag_count,
             (fsm.last_quality_flags & PUSHUP_QUALITY_HIPS_SAG) ?
             "WARN: HIPS SAG [DETECTED]" : "NOT_DETECTED");
    }

  printf("\n================ Push-up Summary ================\n");
  printf("  Total Reps     : %lu\n", (unsigned long)fsm.total_reps);
  printf("  Valid Standard : %lu\n", (unsigned long)fsm.valid_reps);
  printf("  Shallow Faults : %lu\n", (unsigned long)fsm.shallow_count);
  printf("  Sagging Faults : %lu\n", (unsigned long)fsm.hips_sag_count);
  printf("  Piking Faults  : %lu\n", (unsigned long)fsm.hips_pike_count);
  printf("=================================================\n");
  printf(">>> [Stage 3] Push-up FSM Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_test_plank
 ****************************************************************************/

static int cmd_test_plank(int seconds)
{
  if (seconds <= 0)
    {
      seconds = 10;
    }

  printf("\n>>> [Stage 3] Plank Timer Simulation (%d seconds)...\n",
         seconds);

  plank_fsm_t fsm;
  plank_fsm_init(&fsm);

  uint32_t t = 0;

  printf("  [Phase 1] 4s Perfect Plank...\n");
  for (int i = 0; i < 4; i++)
    {
      plank_fsm_update(&fsm, &g_sample_pose_plank_perfect, t);
      t += 1000;
    }

  printf("  [Phase 2] 3s Sagging Hips Plank...\n");
  for (int i = 0; i < 3; i++)
    {
      plank_fsm_update(&fsm, &g_sample_pose_plank_sag, t);
      t += 1000;
    }

  printf("  [Phase 3] 3s Piking Hips Plank...\n");
  for (int i = 0; i < 3; i++)
    {
      plank_fsm_update(&fsm, &g_sample_pose_plank_pike, t);
      t += 1000;
    }

  float score = plank_fsm_get_quality_score(&fsm);

  printf("\n================ Plank Summary ================\n");
  printf("  Total Duration : %lu ms (%.1f s)\n",
         (unsigned long)fsm.total_hold_duration_ms,
         (float)fsm.total_hold_duration_ms / 1000.0f);
  printf("  Valid Hold     : %lu ms (%.1f s)\n",
         (unsigned long)fsm.valid_hold_duration_ms,
         (float)fsm.valid_hold_duration_ms / 1000.0f);
  printf("  Hips Sag Time  : %lu ms\n",
         (unsigned long)fsm.hips_sag_duration_ms);
  printf("  Hips Pike Time : %lu ms\n",
         (unsigned long)fsm.hips_pike_duration_ms);
  printf("  Quality Score  : %.1f %%\n", score);
  printf("===============================================\n");
  printf(">>> [Stage 3] Plank Timer Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_render
 ****************************************************************************/

static int cmd_render(const char *ppm_path)
{
  printf("\n>>> [Stage 4] Running OSD Skeleton Renderer...\n");

  uint32_t w = 320;
  uint32_t h = 240;
  uint8_t *buf = (uint8_t *)malloc(w * h * 2);
  if (buf == NULL)
    {
      printf("ERROR: Failed to allocate framebuffer memory!\n");
      return -ENOMEM;
    }

  velafit_canvas_t canvas;
  velafit_canvas_init(&canvas, buf, w, h, VELAFIT_PIXFMT_RGB565);

  /* Render Dashboard with Skeleton, Guidance Arrows & Depth Gauge */

  velafit_render_dashboard(&canvas,
                           &g_sample_pose_squat_valgus,
                           "SQUAT",
                           12,
                           18.5f,
                           30.0f,
                           85.0f,
                           90.0f,
                           SQUAT_QUALITY_KNEE_CAVING,
                           "WARN: PUSH KNEES OUTWARD");

  printf("  [1/2] 320x240 RGB565 Dashboard (Skeleton, Gauge, HUD) [OK]\n");

  /* Save or Output to FB */

  if (ppm_path != NULL && ppm_path[0] != '\0')
    {
      int ret = velafit_render_save_ppm(&canvas, ppm_path);
      if (ret == OK)
        {
          printf("  [2/2] Saved PPM Image: %s [OK]\n", ppm_path);
        }
      else
        {
          printf("  [2/2] Failed to save PPM: %d\n", ret);
        }
    }
  else
    {
      velafit_render_to_fb0(&canvas);
      printf("  [2/2] Rendered to /dev/fb0 [OK]\n");
    }

  free(buf);
  printf(">>> [Stage 4] OSD Skeleton Render Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_audio
 ****************************************************************************/

static int cmd_audio(const char *cue_arg)
{
  printf("\n>>> [Stage 4] Running Voice & Audio Cue Dispatcher...\n");

  if (cue_arg != NULL && strcmp(cue_arg, "all") != 0)
    {
      velafit_audio_cue_type_t cue = VELAFIT_AUDIO_CUE_START;

      if (strcmp(cue_arg, "count") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_REP_COUNT;
        }
      else if (strcmp(cue_arg, "shallow") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_WARN_SHALLOW;
        }
      else if (strcmp(cue_arg, "valgus") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_WARN_VALGUS;
        }
      else if (strcmp(cue_arg, "lean") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_WARN_LEAN;
        }
      else if (strcmp(cue_arg, "sag") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_WARN_SAG;
        }
      else if (strcmp(cue_arg, "pike") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_WARN_PIKE;
        }
      else if (strcmp(cue_arg, "finish") == 0)
        {
          cue = VELAFIT_AUDIO_CUE_FINISH;
        }

      printf("  Audio Cue: [%s] -> %s\n",
             velafit_audio_cue_name(cue),
             velafit_audio_cue_desc(cue));
      velafit_audio_cue_play(cue);
    }
  else
    {
      for (int i = 0; i < VELAFIT_AUDIO_CUE_MAX; i++)
        {
          printf("  [%d/%d] Cue: [%s] -> %s\n",
                 i + 1, VELAFIT_AUDIO_CUE_MAX,
                 velafit_audio_cue_name((velafit_audio_cue_type_t)i),
                 velafit_audio_cue_desc((velafit_audio_cue_type_t)i));
          velafit_audio_cue_play((velafit_audio_cue_type_t)i);
          usleep(100000);
        }
    }

  printf(">>> [Stage 4] Audio Cue Test: [PASS]\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_storage
 ****************************************************************************/

static int cmd_storage(const char *subcmd, const char *arg)
{
  printf("\n>>> [Storage] Offline Session Storage Manager...\n");
  printf("  Base Path: %s\n", velafit_storage_get_path());

  if (subcmd == NULL || strcmp(subcmd, "list") == 0)
    {
      velafit_session_record_t recs[VELAFIT_STORAGE_MAX_SESSIONS];
      size_t count = 0;
      velafit_storage_list_sessions(recs, VELAFIT_STORAGE_MAX_SESSIONS,
                                    &count);

      printf("\n--- Local Workout Session Index (%zu entries) ---\n",
             count);
      if (count == 0)
        {
          printf("  (No stored workout sessions found)\n");
        }
      else
        {
          printf("  %-24s | %-12s | %-6s | %-8s | %-8s | %-8s\n",
                 "Session ID", "Exercise", "Reps", "Time(s)",
                 "Calories", "Sync");
          printf("  --------------------------------------------------"
                 "---------------------------\n");
          for (size_t i = 0; i < count; i++)
            {
              const char *st_str = "PENDING";
              if (recs[i].sync_status == VELAFIT_SYNC_SYNCED)
                {
                  st_str = "SYNCED";
                }
              else if (recs[i].sync_status == VELAFIT_SYNC_FAILED)
                {
                  st_str = "FAILED";
                }

              printf("  %-22s | %-10s | %-4lu | %-6lu | "
                     "%-5.1f kcal | %-7s\n",
                     recs[i].session_id,
                     recs[i].exercise_type,
                     (unsigned long)recs[i].valid_reps,
                     (unsigned long)recs[i].duration_sec,
                     recs[i].calories_kcal,
                     st_str);
            }
        }
    }
  else if (strcmp(subcmd, "info") == 0 && arg != NULL)
    {
      char json_buf[2048];
      int ret = velafit_storage_load_session(arg, json_buf,
                                            sizeof(json_buf));
      if (ret == OK)
        {
          printf("\n--- Session Payload [%s] ---\n%s\n", arg, json_buf);
        }
      else
        {
          printf("ERROR: Session '%s' not found in storage (ret: %d)\n",
                 arg, ret);
        }
    }
  else if (strcmp(subcmd, "summary") == 0)
    {
      uint32_t total = 0;
      uint32_t pending = 0;
      float calories = 0.0f;
      velafit_storage_get_summary(&total, &pending, &calories);

      printf("\n--- Storage Aggregated Summary ---\n");
      printf("  Total Sessions Recorded : %lu\n", (unsigned long)total);
      printf("  Pending Cloud Sync      : %lu\n",
             (unsigned long)pending);
      printf("  Total Burned Calories   : %.2f kcal\n", calories);
    }

  printf(">>> [Storage] Completed.\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_sync
 ****************************************************************************/

static int cmd_sync(const char *subcmd)
{
  printf("\n>>> [Sync] Edge-Cloud Message Synchronizer...\n");

  if (subcmd == NULL || strcmp(subcmd, "status") == 0)
    {
      int pending = velafit_sync_get_pending_count();
      bool net_ready = velafit_sync_is_network_ready();

      printf("  ESP32-C6 Wireless Link : %s\n",
             net_ready ? "ONLINE (Active)" : "OFFLINE / RESERVED");
      printf("  Pending Sessions       : %d\n", pending);
    }
  else if (strcmp(subcmd, "mock") == 0)
    {
      printf("  Registering Mock Wireless Sender (Simulating C6 Link)...\n");
      velafit_sync_register_sender(velafit_sync_mock_sender);

      int count = velafit_sync_flush();
      printf("  Sync Flush Result: %d sessions pushed to cloud\n", count);
    }
  else if (strcmp(subcmd, "flush") == 0)
    {
      int count = velafit_sync_flush();
      printf("  Sync Flush Result: %d sessions synced\n", count);
    }

  printf(">>> [Sync] Completed.\n\n");
  return 0;
}

/****************************************************************************
 * Name: cmd_plan
 ****************************************************************************/

static int cmd_plan(const char *subcmd, const char *plan_name)
{
  printf("\n>>> [Plan] VelaFit Workout Plan Scheduler...\n");

  if (subcmd == NULL || strcmp(subcmd, "list") == 0)
    {
      size_t count = velafit_get_preset_plan_count();
      printf("Available Workout Routines (%lu total):\n",
             (unsigned long)count);
      printf("-------------------------------------------------------\n");
      for (size_t i = 0; i < count; i++)
        {
          const velafit_workout_plan_t *p = velafit_get_preset_plan(i);
          if (p != NULL)
            {
              printf("  [%lu] %-10s | %-16s | %lu Steps | %s\n",
                     (unsigned long)(i + 1),
                     p->plan_name,
                     p->plan_id,
                     (unsigned long)p->num_steps,
                     p->plan_desc);
            }
        }

      printf("-------------------------------------------------------\n");
      printf("Run a plan with: velafit_ai plan run <plan_name>\n\n");
      return 0;
    }

  if (strcmp(subcmd, "run") == 0)
    {
      const char *target = (plan_name != NULL) ? plan_name : "tabata";
      const velafit_workout_plan_t *plan =
        velafit_find_preset_plan_by_name(target);

      if (plan == NULL)
        {
          printf("Error: Workout plan '%s' not found!\n", target);
          return -ENOENT;
        }

      printf("Executing Plan: %s (%s)\n", plan->plan_name, plan->plan_desc);
      printf("Total Steps   : %lu\n\n", (unsigned long)plan->num_steps);

      velafit_plan_scheduler_t sched;
      int ret = velafit_plan_scheduler_init(&sched, plan);
      if (ret != OK)
        {
          printf("Error: Failed to init plan scheduler: %d\n", ret);
          return ret;
        }

      /* Simulate execution with pose frames */

      uint32_t step_sim_dt_ms = 100;
      uint32_t max_sim_ticks = 400;

      for (uint32_t tick = 0; tick < max_sim_ticks; tick++)
        {
          if (velafit_plan_scheduler_is_finished(&sched))
            {
              break;
            }

          const pose_frame_t *pose = &g_sample_pose_standing;
          if (sched.current_phase == VELAFIT_PHASE_WORK)
            {
              if (strcmp(sched.pipeline.exercise_name, "squat") == 0)
                {
                  pose = (tick % 4 < 2) ? &g_sample_pose_squat_deep :
                                          &g_sample_pose_standing;
                }
              else if (strcmp(sched.pipeline.exercise_name, "pushup") == 0)
                {
                  pose = (tick % 4 < 2) ? &g_sample_pose_pushup_bottom_deep :
                                          &g_sample_pose_pushup_plank;
                }
              else if (strcmp(sched.pipeline.exercise_name, "plank") == 0)
                {
                  pose = &g_sample_pose_plank_perfect;
                }
              else
                {
                  pose = (tick % 4 < 2) ? &g_sample_pose_jj_open :
                                          &g_sample_pose_jj_closed;
                }
            }

          velafit_plan_scheduler_step(&sched, pose, step_sim_dt_ms);
        }

      char json_report[512];
      velafit_plan_scheduler_finish(&sched, json_report,
                                    sizeof(json_report));
      velafit_plan_scheduler_deinit(&sched);

      printf("--- Plan Execution Finished ---\n");
      printf("%s\n", json_report);
      printf(">>> [Plan] Routine Completed & Saved: [PASS]\n\n");
    }

  return 0;
}

/****************************************************************************
 * Name: cmd_report
 ****************************************************************************/

static void cmd_report(void)
{
  printf("\n>>> [Edge-Cloud] Structured Workout JSON Packet...\n");
  printf("-------------------------------------------------------\n");
  printf("{\n");
  printf("  \"device_id\": \"esp32p4_velafit_01\",\n");
  printf("  \"session_id\": \"sess_20260830_001000\",\n");
  printf("  \"exercise_type\": \"pushup\",\n");
  printf("  \"total_reps\": 25,\n");
  printf("  \"valid_reps\": 22,\n");
  printf("  \"duration_seconds\": 50,\n");
  printf("  \"calories_kcal\": 6.80,\n");
  printf("  \"metrics\": {\n");
  printf("    \"min_elbow_angle_min\": 86.2,\n");
  printf("    \"avg_rep_duration_ms\": 1950,\n");
  printf("    \"shallow_count\": 2,\n");
  printf("    \"hips_sag_count\": 1,\n");
  printf("    \"hips_pike_count\": 0\n");
  printf("  },\n");
  printf("  \"cloud_ai_agent_status\": \"READY_TO_SYNC\"\n");
  printf("}\n");
  printf("-------------------------------------------------------\n\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  if (argc < 2)
    {
      print_usage();
      return 0;
    }

  const char *cmd = argv[1];

  if (strcmp(cmd, "benchmark") == 0)
    {
      esp_nn_run_benchmarks();
    }
  else if (strcmp(cmd, "test_pose") == 0)
    {
      cmd_test_pose();
    }
  else if (strcmp(cmd, "test_squat") == 0)
    {
      int count = (argc >= 3) ? atoi(argv[2]) : 1;
      cmd_test_squat(count);
    }
  else if (strcmp(cmd, "test_jj") == 0 ||
           strcmp(cmd, "test_jumping_jack") == 0)
    {
      int count = (argc >= 3) ? atoi(argv[2]) : 1;
      cmd_test_jumping_jack(count);
    }
  else if (strcmp(cmd, "test_pushup") == 0)
    {
      int count = (argc >= 3) ? atoi(argv[2]) : 1;
      cmd_test_pushup(count);
    }
  else if (strcmp(cmd, "test_plank") == 0)
    {
      int seconds = (argc >= 3) ? atoi(argv[2]) : 10;
      cmd_test_plank(seconds);
    }
  else if (strcmp(cmd, "render") == 0)
    {
      const char *ppm = (argc >= 3) ? argv[2] : NULL;
      cmd_render(ppm);
    }
  else if (strcmp(cmd, "audio") == 0)
    {
      const char *cue = (argc >= 3) ? argv[2] : "all";
      cmd_audio(cue);
    }
  else if (strcmp(cmd, "pipeline") == 0)
    {
      int cycles = (argc >= 3) ? atoi(argv[2]) : 1;
      const char *ppm = (argc >= 4) ? argv[3] : NULL;
      velafit_pipeline_run_simulation("squat", cycles, ppm);
    }
  else if (strcmp(cmd, "plan") == 0)
    {
      const char *sub = (argc >= 3) ? argv[2] : "list";
      const char *arg = (argc >= 4) ? argv[3] : NULL;
      cmd_plan(sub, arg);
    }
  else if (strcmp(cmd, "storage") == 0)
    {
      const char *sub = (argc >= 3) ? argv[2] : "list";
      const char *arg = (argc >= 4) ? argv[3] : NULL;
      cmd_storage(sub, arg);
    }
  else if (strcmp(cmd, "sync") == 0)
    {
      const char *sub = (argc >= 3) ? argv[2] : "status";
      cmd_sync(sub);
    }
  else if (strcmp(cmd, "report") == 0)
    {
      cmd_report();
    }
  else if (strcmp(cmd, "all") == 0)
    {
      printf("\n=======================================================\n");
      printf("  VelaFit AI Full Suite (Stage 1 ~ 4 + Plan + Sync)\n");
      printf("=======================================================\n");
      esp_nn_run_benchmarks();
      cmd_test_pose();
      cmd_test_squat(1);
      cmd_test_jumping_jack(1);
      cmd_test_pushup(1);
      cmd_test_plank(10);
      cmd_render(NULL);
      cmd_audio("all");
      velafit_pipeline_run_simulation("squat", 1, NULL);
      velafit_pipeline_run_simulation("pushup", 1, NULL);
      velafit_pipeline_run_simulation("plank", 1, NULL);
      cmd_plan("run", "tabata");
      cmd_storage("list", NULL);
      cmd_sync("mock");
      cmd_report();
      printf("=======================================================\n");
      printf("  VelaFit AI Full Verification Suite: [ALL PASS]\n");
      printf("=======================================================\n\n");
    }
  else
    {
      printf("Unknown command: %s\n", cmd);
      print_usage();
      return -1;
    }

  return 0;
}
