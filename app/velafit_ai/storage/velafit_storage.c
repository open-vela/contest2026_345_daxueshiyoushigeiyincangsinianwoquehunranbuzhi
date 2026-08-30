/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/storage/velafit_storage.c
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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "velafit_storage.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char g_storage_base_path[128] = "";
static velafit_session_record_t g_sessions[VELAFIT_STORAGE_MAX_SESSIONS];
static size_t g_num_sessions = 0;
static bool g_storage_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: save_index_file
 ****************************************************************************/

static int save_index_file(void)
{
  char index_path[160];
  snprintf(index_path, sizeof(index_path), "%s/index.bin",
           g_storage_base_path);

  FILE *fp = fopen(index_path, "wb");
  if (fp == NULL)
    {
      return -errno;
    }

  uint32_t count = (uint32_t)g_num_sessions;
  fwrite(&count, sizeof(uint32_t), 1, fp);
  if (count > 0)
    {
      fwrite(g_sessions, sizeof(velafit_session_record_t), count, fp);
    }

  fclose(fp);
  return OK;
}

/****************************************************************************
 * Name: load_index_file
 ****************************************************************************/

static int load_index_file(void)
{
  char index_path[160];
  snprintf(index_path, sizeof(index_path), "%s/index.bin",
           g_storage_base_path);

  FILE *fp = fopen(index_path, "rb");
  if (fp == NULL)
    {
      g_num_sessions = 0;
      return OK;
    }

  uint32_t count = 0;
  if (fread(&count, sizeof(uint32_t), 1, fp) != 1)
    {
      fclose(fp);
      g_num_sessions = 0;
      return OK;
    }

  if (count > VELAFIT_STORAGE_MAX_SESSIONS)
    {
      count = VELAFIT_STORAGE_MAX_SESSIONS;
    }

  size_t read_cnt = fread(g_sessions, sizeof(velafit_session_record_t),
                          count, fp);
  g_num_sessions = read_cnt;
  fclose(fp);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_storage_init
 ****************************************************************************/

int velafit_storage_init(const char *base_path)
{
  char sess_dir[160];
  int ret;

  if (base_path != NULL && base_path[0] != '\0')
    {
      strncpy(g_storage_base_path, base_path,
              sizeof(g_storage_base_path) - 1);
    }
  else
    {
      /* Check if /sdcard is mounted */

      if (access("/sdcard", F_OK) == 0 &&
          mkdir(VELAFIT_STORAGE_SDCARD_PATH, 0777) == 0)
        {
          strncpy(g_storage_base_path, VELAFIT_STORAGE_SDCARD_PATH,
                  sizeof(g_storage_base_path) - 1);
        }
      else if (access(VELAFIT_STORAGE_SDCARD_PATH, F_OK) == 0)
        {
          strncpy(g_storage_base_path, VELAFIT_STORAGE_SDCARD_PATH,
                  sizeof(g_storage_base_path) - 1);
        }
      else
        {
          strncpy(g_storage_base_path, VELAFIT_STORAGE_DEFAULT_PATH,
                  sizeof(g_storage_base_path) - 1);
        }
    }

  /* Create base directory */

  ret = mkdir(g_storage_base_path, 0777);
  if (ret != 0 && errno != EEXIST)
    {
      /* Fallback to /tmp */

      strncpy(g_storage_base_path, VELAFIT_STORAGE_FALLBACK_PATH,
              sizeof(g_storage_base_path) - 1);
      mkdir(g_storage_base_path, 0777);
    }

  /* Create sessions directory */

  snprintf(sess_dir, sizeof(sess_dir), "%s/sessions",
           g_storage_base_path);
  mkdir(sess_dir, 0777);

  load_index_file();
  g_storage_initialized = true;
  return OK;
}

/****************************************************************************
 * Name: velafit_storage_get_path
 ****************************************************************************/

const char *velafit_storage_get_path(void)
{
  if (!g_storage_initialized)
    {
      velafit_storage_init(NULL);
    }

  return g_storage_base_path;
}

/****************************************************************************
 * Name: velafit_storage_save_session
 ****************************************************************************/

int velafit_storage_save_session(const velafit_session_record_t *rec,
                                 const char *json_payload)
{
  if (rec == NULL || rec->session_id[0] == '\0')
    {
      return -EINVAL;
    }

  if (!g_storage_initialized)
    {
      velafit_storage_init(NULL);
    }

  /* 1. Save JSON Payload File */

  if (json_payload != NULL && json_payload[0] != '\0')
    {
      char file_path[180];
      snprintf(file_path, sizeof(file_path), "%s/sessions/%s.json",
               g_storage_base_path, rec->session_id);

      FILE *fp = fopen(file_path, "w");
      if (fp != NULL)
        {
          fputs(json_payload, fp);
          fclose(fp);
        }
    }

  /* 2. Update In-Memory Index */

  int found_idx = -1;
  for (size_t i = 0; i < g_num_sessions; i++)
    {
      if (strcmp(g_sessions[i].session_id, rec->session_id) == 0)
        {
          found_idx = (int)i;
          break;
        }
    }

  if (found_idx >= 0)
    {
      memcpy(&g_sessions[found_idx], rec,
             sizeof(velafit_session_record_t));
    }
  else
    {
      if (g_num_sessions < VELAFIT_STORAGE_MAX_SESSIONS)
        {
          memcpy(&g_sessions[g_num_sessions], rec,
                 sizeof(velafit_session_record_t));
          g_num_sessions++;
        }
      else
        {
          /* Shift array to make room for newest session */

          memmove(&g_sessions[0], &g_sessions[1],
                  sizeof(velafit_session_record_t) *
                  (VELAFIT_STORAGE_MAX_SESSIONS - 1));
          memcpy(&g_sessions[VELAFIT_STORAGE_MAX_SESSIONS - 1], rec,
                 sizeof(velafit_session_record_t));
        }
    }

  /* 3. Persist Index */

  return save_index_file();
}

/****************************************************************************
 * Name: velafit_storage_load_session
 ****************************************************************************/

int velafit_storage_load_session(const char *session_id,
                                 char *json_buf,
                                 size_t max_len)
{
  if (session_id == NULL || json_buf == NULL || max_len == 0)
    {
      return -EINVAL;
    }

  if (!g_storage_initialized)
    {
      velafit_storage_init(NULL);
    }

  char file_path[180];
  snprintf(file_path, sizeof(file_path), "%s/sessions/%s.json",
           g_storage_base_path, session_id);

  FILE *fp = fopen(file_path, "r");
  if (fp == NULL)
    {
      return -ENOENT;
    }

  size_t bytes = fread(json_buf, 1, max_len - 1, fp);
  json_buf[bytes] = '\0';
  fclose(fp);
  return OK;
}

/****************************************************************************
 * Name: velafit_storage_list_sessions
 ****************************************************************************/

int velafit_storage_list_sessions(velafit_session_record_t *records,
                                  size_t max_count,
                                  size_t *actual_count)
{
  if (records == NULL || actual_count == NULL)
    {
      return -EINVAL;
    }

  if (!g_storage_initialized)
    {
      velafit_storage_init(NULL);
    }

  size_t count = g_num_sessions;
  if (count > max_count)
    {
      count = max_count;
    }

  memcpy(records, g_sessions, sizeof(velafit_session_record_t) * count);
  *actual_count = count;
  return OK;
}

/****************************************************************************
 * Name: velafit_storage_update_status
 ****************************************************************************/

int velafit_storage_update_status(const char *session_id,
                                  velafit_sync_status_t status)
{
  if (session_id == NULL)
    {
      return -EINVAL;
    }

  if (!g_storage_initialized)
    {
      velafit_storage_init(NULL);
    }

  for (size_t i = 0; i < g_num_sessions; i++)
    {
      if (strcmp(g_sessions[i].session_id, session_id) == 0)
        {
          g_sessions[i].sync_status = status;
          return save_index_file();
        }
    }

  return -ENOENT;
}

/****************************************************************************
 * Name: velafit_storage_get_summary
 ****************************************************************************/

int velafit_storage_get_summary(uint32_t *total_sessions,
                                uint32_t *pending_count,
                                float *total_calories)
{
  if (!g_storage_initialized)
    {
      velafit_storage_init(NULL);
    }

  uint32_t n_sess = 0;
  uint32_t n_pending = 0;
  float calories = 0.0f;

  for (size_t i = 0; i < g_num_sessions; i++)
    {
      n_sess++;
      if (g_sessions[i].sync_status == VELAFIT_SYNC_PENDING ||
          g_sessions[i].sync_status == VELAFIT_SYNC_FAILED)
        {
          n_pending++;
        }

      calories += g_sessions[i].calories_kcal;
    }

  if (total_sessions != NULL)
    {
      *total_sessions = n_sess;
    }

  if (pending_count != NULL)
    {
      *pending_count = n_pending;
    }

  if (total_calories != NULL)
    {
      *total_calories = calories;
    }

  return OK;
}
