/****************************************************************************
 * app/velafit_ai/render/velafit_ui.h
 *
 * Product pages for the VelaFit demo: idle, training, summary and advice.
 * Every page consumes one display snapshot; it never runs inference, network
 * requests or counters. A page only reads and rasterizes.
 ****************************************************************************/

#ifndef __APP_VELAFIT_AI_RENDER_VELAFIT_UI_H
#define __APP_VELAFIT_AI_RENDER_VELAFIT_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "velafit_render.h"

typedef enum
{
  VELAFIT_UI_PAGE_IDLE = 0,
  VELAFIT_UI_PAGE_TRAINING,
  VELAFIT_UI_PAGE_SUMMARY,
  VELAFIT_UI_PAGE_ADVICE
} velafit_ui_page_t;

typedef enum
{
  VELAFIT_UI_TONE_NORMAL = 0,
  VELAFIT_UI_TONE_WARN,
  VELAFIT_UI_TONE_ERROR
} velafit_ui_tone_t;

typedef struct
{
  velafit_ui_page_t page;

  /* Data provenance. DEMO must stay visible whenever it is set (U7). */
  bool demo_data;

  /* Real training state */
  bool tracking;
  uint32_t reps;
  uint32_t target;
  uint32_t good_reps;
  bool good_valid;
  uint32_t warn_reps;
  bool warn_valid;
  uint32_t elapsed_sec;
  bool elapsed_valid;
  bool countdown;
  uint32_t countdown_number;

  /* One line per page, already resolved by the caller's priority rules */
  const char *title;
  const char *state_label;
  const char *hint;
  velafit_ui_tone_t hint_tone;

  /* Advice page body (ASCII until a CJK font is delivered) */
  const char *advice;
  const char *advice_status;
} velafit_ui_snapshot_t;

#ifdef __cplusplus
extern "C"
{
#endif

/* Rasterize one page. Training pages keep the live preview untouched. */
void velafit_ui_render(velafit_canvas_t *canvas,
                       const velafit_ui_snapshot_t *snapshot);

/* Fill the whole canvas with the theme background. */
void velafit_ui_paint_background(velafit_canvas_t *canvas);

#ifdef __cplusplus
}
#endif

#endif /* __APP_VELAFIT_AI_RENDER_VELAFIT_UI_H */
