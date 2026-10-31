/****************************************************************************
 * app/velafit_ai/render/velafit_ui.c
 *
 * Product page rasterizer for the VelaFit demo.
 *
 * Pages are pure functions of a display snapshot: no inference, no network,
 * no counter mutation. All colors, bands, type scales and spacing come from
 * velafit_ui_theme.h.
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "velafit_ui.h"
#include "velafit_ui_theme.h"

/****************************************************************************
 * Private Helpers
 ****************************************************************************/

static int ui_text_width(const char *text, int scale)
{
  size_t length;

  if (text == NULL)
    {
      return 0;
    }

  length = strlen(text);
  if (length == 0)
    {
      return 0;
    }

  return (int)((length * VF_UI_GLYPH_ADV - 1) * (size_t)scale);
}

/* Copy at most max_chars glyphs. Longer text is truncated with "..." so a
 * page can never rely on clipping to hide an overflow. */

static void ui_copy_fit(char *dst, size_t cap, const char *src,
                        size_t max_chars)
{
  size_t length;

  if (cap == 0)
    {
      return;
    }

  if (src == NULL)
    {
      dst[0] = '\0';
      return;
    }

  length = strlen(src);
  if (max_chars >= cap)
    {
      max_chars = cap - 1;
    }

  if (length <= max_chars)
    {
      memcpy(dst, src, length + 1);
      return;
    }

  if (max_chars <= 3)
    {
      dst[0] = '\0';
      return;
    }

  memcpy(dst, src, max_chars - 3);
  memcpy(dst + max_chars - 3, "...", 4);
}

/* Draw text centered on (cx, cy) in framebuffer coordinates. velafit_draw_string
 * takes the top-left corner of the unrotated box, so recenter here. */

static void ui_draw_center(velafit_canvas_t *canvas, int cx, int cy,
                           const char *text, velafit_color_t fg,
                           velafit_color_t bg, int scale)
{
  int width = ui_text_width(text, scale);

  if (width <= 0)
    {
      return;
    }

  velafit_draw_string(canvas, cx - width / 2, cy - (VF_UI_GLYPH_H * scale) / 2,
                      text, fg, bg, scale);
}

static velafit_color_t ui_tone_color(velafit_ui_tone_t tone)
{
  switch (tone)
    {
      case VELAFIT_UI_TONE_WARN:
        return VF_UI_WARN;

      case VELAFIT_UI_TONE_ERROR:
        return VF_UI_ERROR;

      case VELAFIT_UI_TONE_NORMAL:
      default:
        return VF_UI_ACCENT;
    }
}

static uint32_t ui_count_scale(uint32_t count)
{
  if (count > 99)
    {
      return VF_UI_COUNT_SCALE_3;
    }

  if (count > 9)
    {
      return VF_UI_COUNT_SCALE_2;
    }

  return VF_UI_COUNT_SCALE_1;
}

/****************************************************************************
 * Private Page Parts
 ****************************************************************************/

/* Top band: product title, pose status, permanent DEMO provenance tag. */

static void ui_draw_status_band(velafit_canvas_t *canvas,
                                const velafit_ui_snapshot_t *snapshot,
                                const char *title)
{
  char line[VF_UI_STATUS_MAX_CHARS + 8];
  velafit_color_t state_color;

  velafit_draw_rect_filled(canvas, VF_UI_STATUS_X0, 0,
                           VF_UI_STATUS_X1 - VF_UI_STATUS_X0, VF_UI_SCREEN_H,
                           VF_UI_PANEL);
  velafit_draw_line(canvas, VF_UI_STATUS_X1, 0, VF_UI_STATUS_X1,
                    VF_UI_SCREEN_H - 1, VF_UI_PANEL_EDGE, 2);

  ui_copy_fit(line, sizeof(line), title, VF_UI_HINT_MAX_CHARS);
  ui_draw_center(canvas, 28, 240, line, VF_UI_TEXT, VF_UI_PANEL,
                 VF_UI_TITLE_SCALE);

  if (snapshot->page == VELAFIT_UI_PAGE_TRAINING)
    {
      state_color = snapshot->tracking ? VF_UI_ACCENT : VF_UI_ERROR;
      ui_draw_center(canvas, 28, 540,
                     snapshot->tracking ? "TRACKING" : "NO POSE",
                     state_color, VF_UI_PANEL, VF_UI_BODY_SCALE);
    }

  if (snapshot->demo_data)
    {
      ui_draw_center(canvas, 28, 44, "DEMO", VF_UI_WARN, VF_UI_PANEL,
                     VF_UI_BODY_SCALE);
    }
}

/* Bottom band: exactly one hint, plus its tone as a rule and text color. */

static void ui_draw_hint_band(velafit_canvas_t *canvas,
                              const char *hint, velafit_ui_tone_t tone)
{
  char line[VF_UI_HINT_MAX_CHARS + 8];
  velafit_color_t color = ui_tone_color(tone);
  int width;

  velafit_draw_rect_filled(canvas, VF_UI_HINT_X0, 0,
                           VF_UI_HINT_X1 - VF_UI_HINT_X0, VF_UI_SCREEN_H,
                           VF_UI_PANEL);
  velafit_draw_line(canvas, VF_UI_HINT_X0, 0, VF_UI_HINT_X0,
                    VF_UI_SCREEN_H - 1, color, 2);

  if (hint == NULL || hint[0] == '\0')
    {
      return;
    }

  ui_copy_fit(line, sizeof(line), hint, VF_UI_HINT_MAX_CHARS);

  /* Longest reserved hint must fit inside the band, never clip. */
  width = ui_text_width(line, VF_UI_HINT_SCALE);
  if (width > VF_UI_SCREEN_H - 2 * VF_UI_GAP_M)
    {
      ui_copy_fit(line, sizeof(line), hint,
                  (size_t)((VF_UI_SCREEN_H - 2 * VF_UI_GAP_M) / 6));
    }

  ui_draw_center(canvas, (VF_UI_HINT_X0 + VF_UI_HINT_X1) / 2,
                 VF_UI_SCREEN_H / 2, line, color, VF_UI_PANEL,
                 VF_UI_HINT_SCALE);
}

/* Main count zone. The zone is fixed so 9 -> 10 -> 100 never moves anything. */

static void ui_draw_count_zone(velafit_canvas_t *canvas,
                               const velafit_ui_snapshot_t *snapshot)
{
  char line[VF_UI_STATUS_MAX_CHARS + 8];
  uint32_t scale;

  velafit_draw_rect_filled(canvas, VF_UI_COUNT_X0, VF_UI_COUNT_Y0,
                           VF_UI_COUNT_X1 - VF_UI_COUNT_X0,
                           VF_UI_COUNT_Y1 - VF_UI_COUNT_Y0, VF_UI_PANEL);
  velafit_draw_rect(canvas, VF_UI_COUNT_X0, VF_UI_COUNT_Y0,
                    VF_UI_COUNT_X1 - VF_UI_COUNT_X0,
                    VF_UI_COUNT_Y1 - VF_UI_COUNT_Y0, VF_UI_PANEL_EDGE);

  ui_draw_center(canvas, 588, 300, "REPS", VF_UI_TEXT_DIM, VF_UI_PANEL,
                 VF_UI_BODY_SCALE);

  if (snapshot->countdown)
    {
      snprintf(line, sizeof(line), "%lu",
               (unsigned long)snapshot->countdown_number);
      ui_draw_center(canvas, 650, 300, line, VF_UI_WARN, VF_UI_PANEL,
                     VF_UI_COUNTDOWN_SCALE);
    }
  else
    {
      scale = ui_count_scale(snapshot->reps);
      snprintf(line, sizeof(line), "%lu", (unsigned long)snapshot->reps);
      ui_draw_center(canvas, 650, 300, line,
                     snapshot->tracking ? VF_UI_TEXT : VF_UI_TEXT_DIM,
                     VF_UI_PANEL, (int)scale);
    }

  snprintf(line, sizeof(line), "TARGET %lu", (unsigned long)snapshot->target);
  ui_draw_center(canvas, 742, 300, line, VF_UI_TEXT_DIM, VF_UI_PANEL,
                 VF_UI_TITLE_SCALE);

  if (snapshot->state_label != NULL && snapshot->state_label[0] != '\0')
    {
      ui_copy_fit(line, sizeof(line), snapshot->state_label,
                  VF_UI_STATUS_MAX_CHARS);
      ui_draw_center(canvas, 815, 300, line, VF_UI_ACCENT, VF_UI_PANEL,
                     VF_UI_BODY_SCALE);
    }

  /* Live duration while the set is running; hidden when it is not measured. */
  if (snapshot->elapsed_valid)
    {
      snprintf(line, sizeof(line), "%02lu:%02lu",
               (unsigned long)(snapshot->elapsed_sec / 60),
               (unsigned long)(snapshot->elapsed_sec % 60));
      ui_draw_center(canvas, 858, 300, line, VF_UI_TEXT_DIM, VF_UI_PANEL,
                     VF_UI_BODY_SCALE);
    }
}

/****************************************************************************
 * Private Pages
 ****************************************************************************/

static void ui_page_idle(velafit_canvas_t *canvas,
                         const velafit_ui_snapshot_t *snapshot)
{
  velafit_ui_paint_background(canvas);
  ui_draw_status_band(canvas, snapshot, "VELAFIT");

  ui_draw_center(canvas, 330, 300, "SQUAT", VF_UI_ACCENT, VF_UI_BG,
                 VF_UI_COUNTDOWN_SCALE);
  ui_draw_center(canvas, 440, 300, "TRAINING", VF_UI_TEXT, VF_UI_BG,
                 VF_UI_TITLE_SCALE);
  ui_draw_center(canvas, 500, 300, "PRESS START", VF_UI_TEXT_DIM, VF_UI_BG,
                 VF_UI_TITLE_SCALE);

  ui_draw_hint_band(canvas, snapshot->hint ? snapshot->hint :
                    "READY WHEN YOU ARE", snapshot->hint_tone);
}

static void ui_page_training(velafit_canvas_t *canvas,
                             const velafit_ui_snapshot_t *snapshot)
{
  ui_draw_status_band(canvas, snapshot,
                      snapshot->title ? snapshot->title : "VELAFIT SQUAT");
  ui_draw_count_zone(canvas, snapshot);
  ui_draw_hint_band(canvas, snapshot->hint, snapshot->hint_tone);
}

static void ui_page_summary(velafit_canvas_t *canvas,
                            const velafit_ui_snapshot_t *snapshot)
{
  char line[40];

  velafit_ui_paint_background(canvas);
  ui_draw_status_band(canvas, snapshot, "WORKOUT SUMMARY");

  ui_draw_center(canvas, 240, 300, "REPS", VF_UI_TEXT_DIM, VF_UI_BG,
                 VF_UI_BODY_SCALE);
  snprintf(line, sizeof(line), "%lu", (unsigned long)snapshot->reps);
  ui_draw_center(canvas, 330, 300, line, VF_UI_TEXT, VF_UI_BG,
                 VF_UI_SUMMARY_SCALE);

  snprintf(line, sizeof(line), "TARGET %lu", (unsigned long)snapshot->target);
  ui_draw_center(canvas, 430, 300, line, VF_UI_TEXT, VF_UI_BG,
                 VF_UI_TITLE_SCALE);

  if (snapshot->elapsed_valid)
    {
      snprintf(line, sizeof(line), "DURATION %02lu:%02lu",
               (unsigned long)(snapshot->elapsed_sec / 60),
               (unsigned long)(snapshot->elapsed_sec % 60));
      ui_draw_center(canvas, 490, 300, line, VF_UI_TEXT, VF_UI_BG,
                     VF_UI_TITLE_SCALE);
    }

  /* Fields that were not reliably produced stay hidden, never shown as 0. */
  if (snapshot->good_valid)
    {
      snprintf(line, sizeof(line), "GOOD %lu",
               (unsigned long)snapshot->good_reps);
      ui_draw_center(canvas, 550, 300, line, VF_UI_ACCENT, VF_UI_BG,
                     VF_UI_TITLE_SCALE);
    }

  ui_draw_hint_band(canvas, snapshot->hint ? snapshot->hint :
                    "START AGAIN WHEN READY", snapshot->hint_tone);
}

static void ui_page_advice(velafit_canvas_t *canvas,
                           const velafit_ui_snapshot_t *snapshot)
{
  char line[VF_UI_ADVICE_LINE_CHARS + 8];
  const char *cursor;
  int row;

  velafit_ui_paint_background(canvas);
  ui_draw_status_band(canvas, snapshot, "COACH ADVICE");

  /* Body: fixed pitch on framebuffer X, hard-wrapped at whole glyphs. */
  cursor = snapshot->advice != NULL ? snapshot->advice : "";
  row = 0;
  while (cursor[0] != '\0' && row < VF_UI_ADVICE_MAX_LINES)
    {
      size_t take = 0;
      size_t last_space = 0;

      while (cursor[take] != '\0' && take < VF_UI_ADVICE_LINE_CHARS)
        {
          if (cursor[take] == ' ')
            {
              last_space = take;
            }

          ++take;
        }

      if (cursor[take] != '\0' && last_space > 0)
        {
          take = last_space;
        }

      /* Hard wrap at glyph boundaries: copy exactly the characters that
       * fit this row. Never append an ellipsis, otherwise every row of a
       * long advice paragraph would end with "...". */

      {
        size_t copy = take;

        if (copy >= sizeof(line))
          {
            copy = sizeof(line) - 1;
          }

        memcpy(line, cursor, copy);
        line[copy] = '\0';
      }

      ui_draw_center(canvas, 150 + row * VF_UI_BODY_PITCH, 300, line,
                     VF_UI_TEXT, VF_UI_BG, VF_UI_TITLE_SCALE);

      cursor += take;
      while (cursor[0] == ' ')
        {
          ++cursor;
        }

      ++row;
    }

  if (snapshot->advice == NULL || snapshot->advice[0] == '\0')
    {
      ui_draw_center(canvas, 300, 300, "NO ADVICE YET", VF_UI_TEXT_DIM,
                     VF_UI_BG, VF_UI_TITLE_SCALE);
    }

  ui_draw_hint_band(canvas, snapshot->advice_status, snapshot->hint_tone);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void velafit_ui_paint_background(velafit_canvas_t *canvas)
{
  if (canvas == NULL)
    {
      return;
    }

  velafit_canvas_clear(canvas, VF_UI_BG);
}

void velafit_ui_render(velafit_canvas_t *canvas,
                       const velafit_ui_snapshot_t *snapshot)
{
  if (canvas == NULL || snapshot == NULL)
    {
      return;
    }

  switch (snapshot->page)
    {
      case VELAFIT_UI_PAGE_TRAINING:
        ui_page_training(canvas, snapshot);
        break;

      case VELAFIT_UI_PAGE_SUMMARY:
        ui_page_summary(canvas, snapshot);
        break;

      case VELAFIT_UI_PAGE_ADVICE:
        ui_page_advice(canvas, snapshot);
        break;

      case VELAFIT_UI_PAGE_IDLE:
      default:
        ui_page_idle(canvas, snapshot);
        break;
    }
}
