/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <string.h>
#include "velafit_body_check.h"

void vf_negative_probe(const float *points, float *result)
{
  static const int mapping[] = {5, 6, 11, 12, 13, 14, 15, 16};
  pose_frame_t pose;
  memset(&pose, 0, sizeof(pose));
  unsigned strong = 0, floor = 0;
  for (unsigned i = 0; i < 8; ++i)
    {
      kpt_2d_t *p = &pose.kpts[mapping[i]];
      p->x = fminf(1.f, fmaxf(0.f, points[i*3]));
      p->y = fminf(1.f, fmaxf(0.f, points[i*3+1]));
      p->score = fminf(1.f, fmaxf(0.f, points[i*3+2]));
      strong += p->score >= .30f;
      floor += p->score >= .20f;
    }
  pose.valid = strong >= 6;
  velafit_body_check_t check;
  velafit_body_check_result_t output;
  velafit_body_check_init(&check, NULL);
  result[0] = strong;
  result[1] = floor;
  result[2] = velafit_body_check_update(&check, &pose, &output);
  result[3] = output.body_height;
}
