/****************************************************************************
 * board/contest_board/src/board_touch.c
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

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/wqueue.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/touchscreen.h>

#include "espressif/esp_i2c.h"
#include "board_touch.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Polling interval in ticks (50Hz = 20ms) */

#define TOUCH_POLL_DELAY_TICKS     MSEC2TICK(20)

/* I2C Frequency */

#define TOUCH_I2C_FREQ             400000

/* GT911 Register Definitions (16-bit address) */

#define GT911_I2C_ADDR_PRIMARY     0x5d
#define GT911_I2C_ADDR_SECONDARY   0x14
#define GT911_REG_PID              0x8140
#define GT911_REG_STATUS           0x814e
#define GT911_REG_POINTS           0x8150
#define GT911_POINT_BYTES          8

/* FT5x06 / FT6336 Register Definitions (8-bit address) */

#define FT5X06_I2C_ADDR            0x38
#define FT5X06_REG_GESTURE_ID      0x01
#define FT5X06_REG_TD_STATUS       0x02
#define FT5X06_REG_POINTS          0x03
#define FT5X06_REG_CHIP_ID         0xa3
#define FT5X06_REG_FOCAL_ID        0xa8
#define FT5X06_POINT_BYTES         6

/* CST816S Register Definitions (8-bit address) */

#define CST816S_I2C_ADDR           0x15
#define CST816S_REG_GESTURE        0x01
#define CST816S_REG_FINGER_NUM     0x02
#define CST816S_REG_POINTS         0x03
#define CST816S_REG_CHIP_ID        0xa7

#define TOUCH_MAX_POINTS           5

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct contest_touch_dev_s
{
  struct touch_lowerhalf_s lower;
  struct i2c_master_s      *i2c;
  int                      touch_type;
  uint8_t                  i2c_addr;
  uint16_t                 max_x;
  uint16_t                 max_y;
  bool                     was_touched;
  struct work_s            work;
  mutex_t                  lock;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct contest_touch_dev_s g_touch_dev;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: touch_i2c_read
 ****************************************************************************/

static int touch_i2c_read(FAR struct contest_touch_dev_s *dev,
                          uint16_t reg, bool is_16bit_reg,
                          FAR uint8_t *buffer, size_t buflen)
{
  struct i2c_msg_s msg[2];
  uint8_t reg_buf[2];

  if (is_16bit_reg)
    {
      reg_buf[0] = (uint8_t)((reg >> 8) & 0xff);
      reg_buf[1] = (uint8_t)(reg & 0xff);
      msg[0].addr   = dev->i2c_addr;
      msg[0].flags  = 0;
      msg[0].buffer = reg_buf;
      msg[0].length = 2;
    }
  else
    {
      reg_buf[0] = (uint8_t)(reg & 0xff);
      msg[0].addr   = dev->i2c_addr;
      msg[0].flags  = 0;
      msg[0].buffer = reg_buf;
      msg[0].length = 1;
    }

  msg[0].frequency = TOUCH_I2C_FREQ;

  msg[1].addr      = dev->i2c_addr;
  msg[1].flags     = I2C_M_READ;
  msg[1].buffer    = buffer;
  msg[1].length    = buflen;
  msg[1].frequency = TOUCH_I2C_FREQ;

  return I2C_TRANSFER(dev->i2c, msg, 2);
}

/****************************************************************************
 * Name: touch_i2c_write
 ****************************************************************************/

static int touch_i2c_write(FAR struct contest_touch_dev_s *dev,
                           uint16_t reg, bool is_16bit_reg,
                           FAR const uint8_t *buffer, size_t buflen)
{
  struct i2c_msg_s msg;
  uint8_t write_buf[16];

  if (is_16bit_reg)
    {
      write_buf[0] = (uint8_t)((reg >> 8) & 0xff);
      write_buf[1] = (uint8_t)(reg & 0xff);
      if (buflen > 0 && buffer != NULL)
        {
          memcpy(&write_buf[2], buffer, buflen);
        }

      msg.length = 2 + buflen;
    }
  else
    {
      write_buf[0] = (uint8_t)(reg & 0xff);
      if (buflen > 0 && buffer != NULL)
        {
          memcpy(&write_buf[1], buffer, buflen);
        }

      msg.length = 1 + buflen;
    }

  msg.addr      = dev->i2c_addr;
  msg.flags     = 0;
  msg.buffer    = write_buf;
  msg.frequency = TOUCH_I2C_FREQ;

  return I2C_TRANSFER(dev->i2c, &msg, 1);
}

/****************************************************************************
 * Name: touch_read_gt911
 ****************************************************************************/

static int touch_read_gt911(FAR struct contest_touch_dev_s *dev,
                            FAR struct touch_sample_s *sample)
{
  uint8_t raw_points[TOUCH_MAX_POINTS * GT911_POINT_BYTES];
  uint8_t status = 0;
  uint8_t clear_val = 0;
  uint8_t point_cnt;
  uint8_t i;
  uint64_t now;
  bool is_ready;
  int ret;

  ret = touch_i2c_read(dev, GT911_REG_STATUS, true, &status, 1);
  if (ret < 0)
    {
      return ret;
    }

  is_ready = (status & 0x80) != 0;
  point_cnt = status & 0x0f;

  if (!is_ready)
    {
      return -EAGAIN;
    }

  /* Clear status register */

  touch_i2c_write(dev, GT911_REG_STATUS, true, &clear_val, 1);

  if (point_cnt == 0)
    {
      if (dev->was_touched)
        {
          dev->was_touched = false;
          sample->npoints = 1;
          sample->point[0].id = 0;
          sample->point[0].flags = TOUCH_UP;
          sample->point[0].timestamp = touch_get_time();
          return OK;
        }

      return -EAGAIN;
    }

  if (point_cnt > TOUCH_MAX_POINTS)
    {
      point_cnt = TOUCH_MAX_POINTS;
    }

  ret = touch_i2c_read(dev, GT911_REG_POINTS, true, raw_points,
                       point_cnt * GT911_POINT_BYTES);
  if (ret < 0)
    {
      return ret;
    }

  sample->npoints = point_cnt;
  now = touch_get_time();

  for (i = 0; i < point_cnt; i++)
    {
      FAR const uint8_t *p = &raw_points[i * GT911_POINT_BYTES];
      uint8_t id = p[0];
      uint16_t x = (uint16_t)(p[1] | (p[2] << 8));
      uint16_t y = (uint16_t)(p[3] | (p[4] << 8));
      uint16_t sz = (uint16_t)(p[5] | (p[6] << 8));

      sample->point[i].id = id;
      sample->point[i].flags = dev->was_touched ? TOUCH_MOVE : TOUCH_DOWN;
      sample->point[i].flags |= (TOUCH_ID_VALID | TOUCH_POS_VALID |
                                 TOUCH_PRESSURE_VALID);
      sample->point[i].x = x;
      sample->point[i].y = y;
      sample->point[i].pressure = sz;
      sample->point[i].timestamp = now;
    }

  dev->was_touched = true;
  return OK;
}

/****************************************************************************
 * Name: touch_read_ft5x06
 ****************************************************************************/

static int touch_read_ft5x06(FAR struct contest_touch_dev_s *dev,
                             FAR struct touch_sample_s *sample)
{
  uint8_t raw_data[1 + 1 + TOUCH_MAX_POINTS * FT5X06_POINT_BYTES];
  uint8_t gesture_raw;
  uint8_t point_cnt;
  uint8_t i;
  uint16_t gesture = 0;
  uint64_t now;
  int ret;

  ret = touch_i2c_read(dev, FT5X06_REG_GESTURE_ID, false,
                       raw_data, sizeof(raw_data));
  if (ret < 0)
    {
      return ret;
    }

  gesture_raw = raw_data[0];
  point_cnt   = raw_data[1] & 0x0f;

  if (point_cnt == 0)
    {
      if (dev->was_touched)
        {
          dev->was_touched = false;
          sample->npoints = 1;
          sample->point[0].id = 0;
          sample->point[0].flags = TOUCH_UP;
          sample->point[0].timestamp = touch_get_time();
          return OK;
        }

      return -EAGAIN;
    }

  if (point_cnt > TOUCH_MAX_POINTS)
    {
      point_cnt = TOUCH_MAX_POINTS;
    }

  switch (gesture_raw)
    {
      case 0x10:
        gesture = TOUCH_SLIDE_UP;
        break;

      case 0x14:
        gesture = TOUCH_SLIDE_RIGHT;
        break;

      case 0x18:
        gesture = TOUCH_SLIDE_DOWN;
        break;

      case 0x1c:
        gesture = TOUCH_SLIDE_LEFT;
        break;

      default:
        gesture = 0;
        break;
    }

  sample->npoints = point_cnt;
  now = touch_get_time();

  for (i = 0; i < point_cnt; i++)
    {
      FAR const uint8_t *p = &raw_data[2 + i * FT5X06_POINT_BYTES];
      uint8_t event = (p[0] >> 6) & 0x03;
      uint16_t x = (uint16_t)(((p[0] & 0x0f) << 8) | p[1]);
      uint8_t id = (p[2] >> 4) & 0x0f;
      uint16_t y = (uint16_t)(((p[2] & 0x0f) << 8) | p[3]);

      sample->point[i].id = id;
      if (event == 0)
        {
          sample->point[i].flags = TOUCH_DOWN;
        }
      else if (event == 1)
        {
          sample->point[i].flags = TOUCH_UP;
        }
      else
        {
          sample->point[i].flags = TOUCH_MOVE;
        }

      sample->point[i].flags |= (TOUCH_ID_VALID | TOUCH_POS_VALID);
      if (gesture != 0)
        {
          sample->point[i].flags |= TOUCH_GESTURE_VALID;
        }

      sample->point[i].x = x;
      sample->point[i].y = y;
      sample->point[i].gesture = gesture;
      sample->point[i].timestamp = now;
    }

  dev->was_touched = true;
  return OK;
}

/****************************************************************************
 * Name: touch_read_cst816s
 ****************************************************************************/

static int touch_read_cst816s(FAR struct contest_touch_dev_s *dev,
                              FAR struct touch_sample_s *sample)
{
  uint8_t raw_data[6];
  uint8_t gesture_raw;
  uint8_t fingers;
  uint8_t action;
  uint16_t x;
  uint16_t y;
  uint16_t gesture = 0;
  int ret;

  ret = touch_i2c_read(dev, CST816S_REG_GESTURE, false,
                       raw_data, sizeof(raw_data));
  if (ret < 0)
    {
      return ret;
    }

  gesture_raw = raw_data[0];
  fingers     = raw_data[1];

  if (fingers == 0)
    {
      if (dev->was_touched)
        {
          dev->was_touched = false;
          sample->npoints = 1;
          sample->point[0].id = 0;
          sample->point[0].flags = TOUCH_UP;
          sample->point[0].timestamp = touch_get_time();
          return OK;
        }

      return -EAGAIN;
    }

  x = (uint16_t)(((raw_data[2] & 0x0f) << 8) | raw_data[3]);
  y = (uint16_t)(((raw_data[4] & 0x0f) << 8) | raw_data[5]);
  action = (raw_data[2] >> 6) & 0x03;

  if (gesture_raw == 0x01)
    {
      gesture = TOUCH_SLIDE_UP;
    }
  else if (gesture_raw == 0x02)
    {
      gesture = TOUCH_SLIDE_DOWN;
    }
  else if (gesture_raw == 0x03)
    {
      gesture = TOUCH_SLIDE_LEFT;
    }
  else if (gesture_raw == 0x04)
    {
      gesture = TOUCH_SLIDE_RIGHT;
    }
  else if (gesture_raw == 0x05)
    {
      gesture = TOUCH_SINGLE_CLICK;
    }
  else if (gesture_raw == 0x0b)
    {
      gesture = TOUCH_DOUBLE_CLICK;
    }

  sample->npoints = 1;
  sample->point[0].id = 0;
  sample->point[0].flags = (action == 0) ? TOUCH_DOWN : TOUCH_MOVE;
  sample->point[0].flags |= (TOUCH_ID_VALID | TOUCH_POS_VALID);
  if (gesture != 0)
    {
      sample->point[0].flags |= TOUCH_GESTURE_VALID;
    }

  sample->point[0].x = x;
  sample->point[0].y = y;
  sample->point[0].gesture = gesture;
  sample->point[0].timestamp = touch_get_time();

  dev->was_touched = true;
  return OK;
}

/****************************************************************************
 * Name: touch_worker
 ****************************************************************************/

static void touch_worker(FAR void *arg)
{
  FAR struct contest_touch_dev_s *dev =
    (FAR struct contest_touch_dev_s *)arg;
  uint8_t sample_buf[SIZEOF_TOUCH_SAMPLE_S(TOUCH_MAX_POINTS)];
  FAR struct touch_sample_s *sample =
    (FAR struct touch_sample_s *)sample_buf;
  int ret = -ENOTSUP;

  nxmutex_lock(&dev->lock);

  if (dev->touch_type == CONTEST_TOUCH_TYPE_GT911)
    {
      ret = touch_read_gt911(dev, sample);
    }
  else if (dev->touch_type == CONTEST_TOUCH_TYPE_FT5X06)
    {
      ret = touch_read_ft5x06(dev, sample);
    }
  else if (dev->touch_type == CONTEST_TOUCH_TYPE_CST816S)
    {
      ret = touch_read_cst816s(dev, sample);
    }

  if (ret == OK && dev->lower.priv != NULL)
    {
      touch_event(dev->lower.priv, sample);
    }

  nxmutex_unlock(&dev->lock);

  /* Reschedule next periodic sample */

  work_queue(HPWORK, &dev->work, touch_worker, dev,
             TOUCH_POLL_DELAY_TICKS);
}

/****************************************************************************
 * Name: touch_control
 ****************************************************************************/

static int touch_control(FAR struct touch_lowerhalf_s *lower,
                         int cmd, unsigned long arg)
{
  FAR struct contest_touch_dev_s *dev =
    (FAR struct contest_touch_dev_s *)lower;

  switch (cmd)
    {
      case TSIOC_GETMAXPOINTS:
        *(FAR uint8_t *)arg = TOUCH_MAX_POINTS;
        return OK;

      case TSIOC_GETRESOLUTION:
        {
          FAR struct touch_resolution_s *res =
            (FAR struct touch_resolution_s *)arg;
          res->res_x = dev->max_x;
          res->res_y = dev->max_y;
          return OK;
        }

      default:
        return -ENOTTY;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_touch_get_type
 ****************************************************************************/

int board_touch_get_type(void)
{
  return g_touch_dev.touch_type;
}

/****************************************************************************
 * Name: board_touch_initialize
 ****************************************************************************/

int board_touch_initialize(void)
{
  FAR struct contest_touch_dev_s *dev = &g_touch_dev;
  uint8_t chip_id[4];
  int ret;

  memset(dev, 0, sizeof(struct contest_touch_dev_s));
  memset(chip_id, 0, sizeof(chip_id));
  nxmutex_init(&dev->lock);

  /* 1. Initialize I2C0 Bus */

  dev->i2c = esp_i2cbus_initialize(ESPRESSIF_I2C0);
  if (dev->i2c == NULL)
    {
      ierr("ERROR: Failed to initialize I2C0 for Touchscreen\n");
      return -ENODEV;
    }

  /* 2. Auto-Probe Touchscreen IC */

  /* Probe Goodix GT911 (Primary 0x5D, Secondary 0x14) */

  dev->i2c_addr = GT911_I2C_ADDR_PRIMARY;
  if (touch_i2c_read(dev, GT911_REG_PID, true, chip_id, 3) == OK &&
      (chip_id[0] == '9' || chip_id[0] == 0x39))
    {
      dev->touch_type = CONTEST_TOUCH_TYPE_GT911;
      dev->max_x = 1024;
      dev->max_y = 600;
      iinfo("Found Goodix GT911 Touchscreen at 0x%02x\n", dev->i2c_addr);
    }
  else
    {
      dev->i2c_addr = GT911_I2C_ADDR_SECONDARY;
      if (touch_i2c_read(dev, GT911_REG_PID, true, chip_id, 3) == OK &&
          (chip_id[0] == '9' || chip_id[0] == 0x39))
        {
          dev->touch_type = CONTEST_TOUCH_TYPE_GT911;
          dev->max_x = 1024;
          dev->max_y = 600;
          iinfo("Found Goodix GT911 Touchscreen at 0x%02x\n", dev->i2c_addr);
        }
    }

  /* Probe FocalTech FT5x06 / FT6336 (0x38) */

  if (dev->touch_type == CONTEST_TOUCH_TYPE_NONE)
    {
      dev->i2c_addr = FT5X06_I2C_ADDR;
      if (touch_i2c_read(dev, FT5X06_REG_CHIP_ID, false, chip_id, 1) == OK ||
          touch_i2c_read(dev, FT5X06_REG_FOCAL_ID, false, chip_id, 1) == OK)
        {
          dev->touch_type = CONTEST_TOUCH_TYPE_FT5X06;
          dev->max_x = 800;
          dev->max_y = 480;
          iinfo("Found FocalTech FT5x06 Touchscreen at 0x%02x\n",
                dev->i2c_addr);
        }
    }

  /* Probe Hynitron CST816S (0x15) */

  if (dev->touch_type == CONTEST_TOUCH_TYPE_NONE)
    {
      dev->i2c_addr = CST816S_I2C_ADDR;
      if (touch_i2c_read(dev, CST816S_REG_CHIP_ID, false, chip_id, 1) == OK)
        {
          dev->touch_type = CONTEST_TOUCH_TYPE_CST816S;
          dev->max_x = 480;
          dev->max_y = 480;
          iinfo("Found Hynitron CST816S Touchscreen at 0x%02x\n",
                dev->i2c_addr);
        }
    }

  /* Default fallback to GT911 structure for EV Board MIPI DSI display */

  if (dev->touch_type == CONTEST_TOUCH_TYPE_NONE)
    {
      dev->touch_type = CONTEST_TOUCH_TYPE_GT911;
      dev->i2c_addr = GT911_I2C_ADDR_PRIMARY;
      dev->max_x = 1024;
      dev->max_y = 600;
      iwarn("WARNING: Touch probe fallback to GT911 at 0x%02x\n",
            dev->i2c_addr);
    }

  /* 3. Register Touch Lower-half driver */

  dev->lower.maxpoint = TOUCH_MAX_POINTS;
  dev->lower.control  = touch_control;
  dev->lower.write    = NULL;

  ret = touch_register(&dev->lower, "/dev/input0", 1);
  if (ret < 0)
    {
      ierr("ERROR: Failed to register /dev/input0: %d\n", ret);
      return ret;
    }

  /* 4. Start periodic touch sampling worker */

  work_queue(HPWORK, &dev->work, touch_worker, dev,
             TOUCH_POLL_DELAY_TICKS);

  iinfo("Capacitive Touchscreen successfully registered at /dev/input0\n");
  return OK;
}
