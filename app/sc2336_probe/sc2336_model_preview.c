/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>

#include "sc2336_model_preview.h"

#define MODEL_SIDE 96u

static uint16_t rgb565(const uint8_t *rgb)
{
  return (uint16_t)(((uint16_t)(rgb[0] >> 3) << 11) |
                    ((uint16_t)(rgb[1] >> 2) << 5) |
                    (rgb[2] >> 3));
}

int sc2336_model_preview_blit_ccw_rgb565(const uint8_t *rgb96,
                                         size_t rgb96_len,
                                         uint16_t *framebuffer,
                                         size_t framebuffer_len,
                                         uint16_t width,
                                         uint16_t height,
                                         uint16_t stride,
                                         uint8_t scale)
{
  const uint16_t shown = MODEL_SIDE * scale;
  uint16_t origin_x;
  uint16_t origin_y;

  if (!rgb96 || !framebuffer || rgb96_len < MODEL_SIDE * MODEL_SIDE * 3 ||
      scale == 0 || scale > 6 || stride < width || shown > width ||
      shown > height ||
      framebuffer_len < (size_t)stride * height * sizeof(uint16_t))
    {
      return -EINVAL;
    }

  memset(framebuffer, 0, (size_t)stride * height * sizeof(uint16_t));
  origin_x = (width - shown) / 2;
  origin_y = (height - shown) / 2;

  for (uint16_t source_y = 0; source_y < MODEL_SIDE; source_y++)
    {
      for (uint16_t source_x = 0; source_x < MODEL_SIDE; source_x++)
        {
          /* Forward CCW90 in raster coordinates: (x,y)->(y,95-x). */
          const uint16_t rotated_x = source_y;
          const uint16_t rotated_y = MODEL_SIDE - 1 - source_x;
          const uint16_t color = rgb565(&rgb96[
            ((size_t)source_y * MODEL_SIDE + source_x) * 3]);
          for (uint8_t dy = 0; dy < scale; dy++)
            for (uint8_t dx = 0; dx < scale; dx++)
              framebuffer[(size_t)(origin_y + rotated_y * scale + dy) *
                          stride + origin_x + rotated_x * scale + dx] = color;
        }
    }

  return 0;
}

/* Blit the same RGB96 model image into an arbitrary framebuffer rectangle.
 * The source is sampled in destination space so every target pixel is
 * written exactly once (no gaps when the scale is non-integral). */

int sc2336_model_preview_blit_rect_ccw_rgb565(const uint8_t *rgb96,
                                              size_t rgb96_len,
                                              uint16_t *framebuffer,
                                              size_t framebuffer_len,
                                              uint16_t width,
                                              uint16_t height,
                                              uint16_t stride,
                                              uint16_t dst_x,
                                              uint16_t dst_y,
                                              uint16_t dst_w,
                                              uint16_t dst_h)
{
  if (!rgb96 || !framebuffer || rgb96_len < MODEL_SIDE * MODEL_SIDE * 3 ||
      stride < width || dst_w == 0 || dst_h == 0 ||
      dst_x + dst_w > width || dst_y + dst_h > height ||
      framebuffer_len < (size_t)stride * height * sizeof(uint16_t))
    {
      return -EINVAL;
    }

  for (uint16_t dy = 0; dy < dst_h; dy++)
    {
      const uint16_t rotated_y = (uint16_t)((uint32_t)dy * MODEL_SIDE / dst_h);
      const uint16_t source_x = MODEL_SIDE - 1 - rotated_y;
      uint16_t *row = &framebuffer[(size_t)(dst_y + dy) * stride + dst_x];

      for (uint16_t dx = 0; dx < dst_w; dx++)
        {
          const uint16_t source_y =
            (uint16_t)((uint32_t)dx * MODEL_SIDE / dst_w);
          row[dx] = rgb565(&rgb96[
            ((size_t)source_y * MODEL_SIDE + source_x) * 3]);
        }
    }

  return 0;
}
