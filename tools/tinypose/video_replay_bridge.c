/* SPDX-License-Identifier: Apache-2.0 */
/* Host adapter: reuse product algorithms without modifying firmware sources. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "squat_fsm.h"
#include "geometry.h"

typedef struct
{
  squat_fsm_adaptive_t tracker;
  int capture;
} replay_t;

void *vf_replay_create(void)
{
  replay_t *r = calloc(1, sizeof(*r));
  if (r) squat_fsm_adaptive_init(&r->tracker);
  return r;
}

void vf_replay_free(void *handle) { free(handle); }

void vf_replay_step(void *handle, const float *points, unsigned int ms,
                    int capture, float *result)
{
  static const int mapping[] = {5, 6, 11, 12, 13, 14, 15, 16};
  replay_t *r = handle;
  pose_frame_t pose;
  memset(&pose, 0, sizeof(pose));
  unsigned int strong = 0;
  int ready = 1;
  for (unsigned int i = 0; i < 8; ++i)
    {
      kpt_2d_t *p = &pose.kpts[mapping[i]];
      p->x = fminf(1.0f, fmaxf(0.0f, points[i * 3]));
      p->y = fminf(1.0f, fmaxf(0.0f, points[i * 3 + 1]));
      p->score = fminf(1.0f, fmaxf(0.0f, points[i * 3 + 2]));
      strong += p->score >= 0.30f;
      ready &= p->score >= 0.20f;
    }
  pose.valid = strong >= 6;
  pose.timestamp_ms = ms;
  if (capture && !r->capture)
    squat_fsm_adaptive_begin_capture(&r->tracker);
  r->capture = capture;
  int finished = squat_fsm_adaptive_update(&r->tracker, &pose, ms, 0.20f);
  result[0] = r->tracker.fsm.total_reps;
  result[1] = r->tracker.fsm.valid_reps;
  result[2] = r->tracker.fsm.state;
  result[3] = pose.valid && ready;
  result[4] = (velafit_get_knee_angle_left(&pose) +
               velafit_get_knee_angle_right(&pose)) * 0.5f;
  result[5] = r->tracker.standing_angle;
  result[6] = capture && finished;
  result[7] = r->tracker.fsm.last_quality_flags;
}
