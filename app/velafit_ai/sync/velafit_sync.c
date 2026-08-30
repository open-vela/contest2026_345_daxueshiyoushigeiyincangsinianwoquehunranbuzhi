/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/sync/velafit_sync.c
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

#include "velafit_sync.h"
#include "velafit_storage.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_SYNC_DEFAULT_TOPIC "velafit/v1/sessions/upload"
#define VELAFIT_JSON_BUF_SIZE      2048

/****************************************************************************
 * Private Data
 ****************************************************************************/

static velafit_net_sender_t g_sender_cb = NULL;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_sync_init
 ****************************************************************************/

int velafit_sync_init(void)
{
  g_sender_cb = NULL;
  return velafit_storage_init(NULL);
}

/****************************************************************************
 * Name: velafit_sync_register_sender
 ****************************************************************************/

void velafit_sync_register_sender(velafit_net_sender_t sender)
{
  g_sender_cb = sender;
}

/****************************************************************************
 * Name: velafit_sync_is_network_ready
 ****************************************************************************/

bool velafit_sync_is_network_ready(void)
{
  return (g_sender_cb != NULL);
}

/****************************************************************************
 * Name: velafit_sync_get_pending_count
 ****************************************************************************/

int velafit_sync_get_pending_count(void)
{
  uint32_t pending = 0;
  velafit_storage_get_summary(NULL, &pending, NULL);
  return (int)pending;
}

/****************************************************************************
 * Name: velafit_sync_mock_sender
 ****************************************************************************/

int velafit_sync_mock_sender(const char *topic,
                             const uint8_t *payload,
                             size_t len)
{
  if (topic == NULL || payload == NULL || len == 0)
    {
      return -EINVAL;
    }

  printf("  [MOCK-CLOUD-AGENT] Connecting to Cloud Platform...\n");
  printf("  [MOCK-CLOUD-AGENT] Topic: '%s', Payload: %zu bytes\n",
         topic, len);
  printf("  [MOCK-CLOUD-AGENT] HTTP POST / MQTT PUB -> [200 OK ACCEPTED]\n");
  return OK;
}

/****************************************************************************
 * Name: velafit_sync_flush
 ****************************************************************************/

int velafit_sync_flush(void)
{
  if (g_sender_cb == NULL)
    {
      printf("WARN: No network sender registered (ESP32-C6 link down)\n");
      return -ENETDOWN;
    }

  velafit_session_record_t records[VELAFIT_STORAGE_MAX_SESSIONS];
  size_t count = 0;
  int ret = velafit_storage_list_sessions(records,
                                          VELAFIT_STORAGE_MAX_SESSIONS,
                                          &count);
  if (ret != OK)
    {
      return ret;
    }

  char *json_buf = (char *)malloc(VELAFIT_JSON_BUF_SIZE);
  if (json_buf == NULL)
    {
      return -ENOMEM;
    }

  int synced_count = 0;

  for (size_t i = 0; i < count; i++)
    {
      if (records[i].sync_status == VELAFIT_SYNC_PENDING ||
          records[i].sync_status == VELAFIT_SYNC_FAILED)
        {
          printf(">>> [Sync] Syncing Session: %s (%s, Reps: %lu, "
                 "Calories: %.1f kcal)...\n",
                 records[i].session_id,
                 records[i].exercise_type,
                 (unsigned long)records[i].valid_reps,
                 records[i].calories_kcal);

          int load_ret = velafit_storage_load_session(records[i].session_id,
                                                      json_buf,
                                                      VELAFIT_JSON_BUF_SIZE);
          if (load_ret != OK)
            {
              printf("    Failed to load payload from storage: %d\n",
                     load_ret);
              continue;
            }

          /* Invoke registered network sender callback */

          int send_ret = g_sender_cb(VELAFIT_SYNC_DEFAULT_TOPIC,
                                     (const uint8_t *)json_buf,
                                     strlen(json_buf));
          if (send_ret == OK)
            {
              velafit_storage_update_status(records[i].session_id,
                                            VELAFIT_SYNC_SYNCED);
              printf("    Session %s -> [SYNCED OK]\n",
                     records[i].session_id);
              synced_count++;
            }
          else
            {
              velafit_storage_update_status(records[i].session_id,
                                            VELAFIT_SYNC_FAILED);
              printf("    Session %s -> [SYNC FAILED: %d]\n",
                     records[i].session_id, send_ret);
            }
        }
    }

  free(json_buf);
  return synced_count;
}
