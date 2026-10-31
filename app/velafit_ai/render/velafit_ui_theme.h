/****************************************************************************
 * app/velafit_ai/render/velafit_ui_theme.h
 *
 * VelaFit product UI tokens. Colors, layout bands, type scales, spacing and
 * animation timing live here and nowhere else, so pages cannot drift apart.
 *
 * Framebuffer is 1024x600 RGB565. The panel is mounted rotated 90 degrees,
 * therefore:
 *   - framebuffer X is the physical vertical axis (0 = top of the screen)
 *   - framebuffer Y is the physical horizontal axis
 * Text rasterization already rotates CCW90, so a page authored here reads
 * correctly on the product LCD. Camera/pose coordinates are never touched
 * by these tokens.
 ****************************************************************************/

#ifndef __APP_VELAFIT_AI_RENDER_VELAFIT_UI_THEME_H
#define __APP_VELAFIT_AI_RENDER_VELAFIT_UI_THEME_H

#include "velafit_render.h"

/* Screen and fixed bands (framebuffer coordinates) */

#define VF_UI_SCREEN_W        1024
#define VF_UI_SCREEN_H        600

/* Split layout: the live camera owns the whole top block; the lower block is
 * a solid information area so text never sits on top of the video. */
#define VF_UI_CAMERA_X0       0
#define VF_UI_CAMERA_X1       600
#define VF_UI_CAMERA_Y0       0
#define VF_UI_CAMERA_Y1       600

/* Legacy title strip, kept only for the full-screen advice page. */
#define VF_UI_STATUS_X0       0
#define VF_UI_STATUS_X1       56
#define VF_UI_HINT_X0         900
#define VF_UI_HINT_X1         1024

/* Main count zone fills the lower information block edge to edge. */
#define VF_UI_COUNT_X0        600
#define VF_UI_COUNT_X1        900
#define VF_UI_COUNT_Y0        0
#define VF_UI_COUNT_Y1        600

/* Legacy aliases for the camera block (kept so the probe loop and the
 * skeleton overlay agree on one rectangle). */
#define VF_UI_PREVIEW_X0      VF_UI_CAMERA_X0
#define VF_UI_PREVIEW_X1      VF_UI_CAMERA_X1
#define VF_UI_PREVIEW_Y0      VF_UI_CAMERA_Y0
#define VF_UI_PREVIEW_Y1      VF_UI_CAMERA_Y1

/* Palette: dark neutral background, one accent, warn/error as exceptions. */

#define VF_UI_BG              VELAFIT_COLOR_RGB(13, 15, 19)
#define VF_UI_PANEL           VELAFIT_COLOR_RGB(24, 28, 35)
#define VF_UI_PANEL_EDGE      VELAFIT_COLOR_RGB(52, 60, 72)
#define VF_UI_TEXT            VELAFIT_COLOR_RGB(240, 244, 249)
#define VF_UI_TEXT_DIM        VELAFIT_COLOR_RGB(150, 158, 172)
#define VF_UI_ACCENT          VELAFIT_COLOR_RGB(0, 209, 255)
#define VF_UI_WARN            VELAFIT_COLOR_RGB(255, 196, 0)
#define VF_UI_ERROR           VELAFIT_COLOR_RGB(255, 69, 79)

/* Decorative heartbeat only. Never a measured value: the real page shows
 * this shape without any BPM number. */
#define VF_UI_HEART            VELAFIT_COLOR_RGB(255, 82, 102)

/* 5x7 glyph metrics used by velafit_draw_string */
#define VF_UI_GLYPH_ADV       6
#define VF_UI_GLYPH_H         7

/* Three text levels only: title, body, hint. Counts are their own level. */
#define VF_UI_TITLE_SCALE     3
#define VF_UI_BODY_SCALE      2
#define VF_UI_HINT_SCALE      3

/* Fixed count zone; scale depends on digit count, the zone never moves. */
#define VF_UI_COUNT_SCALE_1   9
#define VF_UI_COUNT_SCALE_2   8
#define VF_UI_COUNT_SCALE_3   7
#define VF_UI_COUNTDOWN_SCALE 9
#define VF_UI_SUMMARY_SCALE   9

/* Spacing scale */
#define VF_UI_GAP_XS          4
#define VF_UI_GAP_S           8
#define VF_UI_GAP_M           16
#define VF_UI_GAP_L           24
#define VF_UI_GAP_XL          32

/* Wrapped body line pitch on the physical horizontal axis */
#define VF_UI_BODY_PITCH      34

/* Animation timing (milliseconds). Pages must use these, not literals. */
#define VF_UI_ANIM_FAST_MS    120
#define VF_UI_ANIM_NORMAL_MS  240
#define VF_UI_HINT_MIN_MS     900
#define VF_UI_GO_BANNER_MS    700
#define VF_UI_COMPLETE_MS     1200
/* Summary stays visible long enough to read before the next set starts. */
#define VF_UI_SUMMARY_HOLD_MS 5000

/* Capacity planning for the longest strings (prevents clipping by design) */
#define VF_UI_HINT_MAX_CHARS    31
#define VF_UI_STATUS_MAX_CHARS  16
#define VF_UI_ADVICE_MAX_LINES  22
#define VF_UI_ADVICE_LINE_CHARS 30

#endif /* __APP_VELAFIT_AI_RENDER_VELAFIT_UI_THEME_H */
