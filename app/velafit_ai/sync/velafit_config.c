/****************************************************************************
 * contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/app/velafit_ai/sync/velafit_config.c
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
#include <sys/stat.h>
#include <errno.h>

#include "velafit_config.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VELAFIT_CONFIG_PRIMARY_FILE   "/data/velafit/config.json"
#define VELAFIT_CONFIG_FALLBACK_FILE  "/tmp/velafit/config.json"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static velafit_config_t g_config;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_config_filepath
 ****************************************************************************/

static const char *get_config_filepath(void)
{
  struct stat st;
  if (stat("/data", &st) == 0 && S_ISDIR(st.st_mode))
    {
      mkdir("/data/velafit", 0777);
      return VELAFIT_CONFIG_PRIMARY_FILE;
    }

  mkdir("/tmp/velafit", 0777);
  return VELAFIT_CONFIG_FALLBACK_FILE;
}

/****************************************************************************
 * Name: load_defaults
 ****************************************************************************/

static void load_defaults(void)
{
  memset(&g_config, 0, sizeof(velafit_config_t));
  strncpy(g_config.base_url, VELAFIT_CONFIG_DEFAULT_URL,
          sizeof(g_config.base_url) - 1);
  strncpy(g_config.pro_model, VELAFIT_CONFIG_DEFAULT_PRO_MDL,
          sizeof(g_config.pro_model) - 1);
  strncpy(g_config.asr_model, VELAFIT_CONFIG_DEFAULT_ASR_MDL,
          sizeof(g_config.asr_model) - 1);
  strncpy(g_config.tts_model, VELAFIT_CONFIG_DEFAULT_TTS_MDL,
          sizeof(g_config.tts_model) - 1);
  g_config.cloud_enabled = true;
  g_config.initialized = true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: velafit_config_init
 ****************************************************************************/

int velafit_config_init(void)
{
  if (g_config.initialized)
    {
      return OK;
    }

  load_defaults();

  const char *path = get_config_filepath();
  FILE *fp = fopen(path, "r");
  if (fp == NULL)
    {
      /* Save defaults if no file exists */

      return velafit_config_save();
    }

  char buf[512];
  size_t read_bytes = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);

  if (read_bytes > 0)
    {
      buf[read_bytes] = '\0';

      char *p_url = strstr(buf, "\"base_url\": \"");
      if (p_url != NULL)
        {
          p_url += 13;
          char *p_end = strchr(p_url, '\"');
          if (p_end != NULL)
            {
              size_t len = p_end - p_url;
              if (len < sizeof(g_config.base_url))
                {
                  strncpy(g_config.base_url, p_url, len);
                  g_config.base_url[len] = '\0';
                }
            }
        }

      char *p_key = strstr(buf, "\"api_key\": \"");
      if (p_key != NULL)
        {
          p_key += 12;
          char *p_end = strchr(p_key, '\"');
          if (p_end != NULL)
            {
              size_t len = p_end - p_key;
              if (len < sizeof(g_config.api_key))
                {
                  strncpy(g_config.api_key, p_key, len);
                  g_config.api_key[len] = '\0';
                }
            }
        }
    }

  return OK;
}

/****************************************************************************
 * Name: velafit_config_get
 ****************************************************************************/

const velafit_config_t *velafit_config_get(void)
{
  if (!g_config.initialized)
    {
      velafit_config_init();
    }

  return &g_config;
}

/****************************************************************************
 * Name: velafit_config_save
 ****************************************************************************/

int velafit_config_save(void)
{
  const char *path = get_config_filepath();
  FILE *fp = fopen(path, "w");
  if (fp == NULL)
    {
      return -errno;
    }

  fprintf(fp, "{\n");
  fprintf(fp, "  \"base_url\": \"%s\",\n", g_config.base_url);
  fprintf(fp, "  \"api_key\": \"%s\",\n", g_config.api_key);
  fprintf(fp, "  \"pro_model\": \"%s\",\n", g_config.pro_model);
  fprintf(fp, "  \"asr_model\": \"%s\",\n", g_config.asr_model);
  fprintf(fp, "  \"tts_model\": \"%s\",\n", g_config.tts_model);
  fprintf(fp, "  \"wifi_ssid\": \"%s\",\n", g_config.wifi_ssid);
  fprintf(fp, "  \"cloud_enabled\": %s\n",
          g_config.cloud_enabled ? "true" : "false");
  fprintf(fp, "}\n");

  fclose(fp);
  return OK;
}

/****************************************************************************
 * Name: velafit_config_set_url
 ****************************************************************************/

int velafit_config_set_url(const char *url)
{
  if (url == NULL || strlen(url) == 0)
    {
      return -EINVAL;
    }

  if (!g_config.initialized)
    {
      velafit_config_init();
    }

  strncpy(g_config.base_url, url, sizeof(g_config.base_url) - 1);
  g_config.base_url[sizeof(g_config.base_url) - 1] = '\0';
  return velafit_config_save();
}

/****************************************************************************
 * Name: velafit_config_set_key
 ****************************************************************************/

int velafit_config_set_key(const char *key)
{
  if (key == NULL)
    {
      return -EINVAL;
    }

  if (!g_config.initialized)
    {
      velafit_config_init();
    }

  strncpy(g_config.api_key, key, sizeof(g_config.api_key) - 1);
  g_config.api_key[sizeof(g_config.api_key) - 1] = '\0';
  return velafit_config_save();
}

/****************************************************************************
 * Name: velafit_config_set_model
 ****************************************************************************/

int velafit_config_set_model(const char *model)
{
  if (model == NULL || strlen(model) == 0)
    {
      return -EINVAL;
    }

  if (!g_config.initialized)
    {
      velafit_config_init();
    }

  strncpy(g_config.pro_model, model, sizeof(g_config.pro_model) - 1);
  g_config.pro_model[sizeof(g_config.pro_model) - 1] = '\0';
  return velafit_config_save();
}

/****************************************************************************
 * Name: velafit_config_set_wifi
 ****************************************************************************/

int velafit_config_set_wifi(const char *ssid, const char *pwd)
{
  if (ssid == NULL)
    {
      return -EINVAL;
    }

  if (!g_config.initialized)
    {
      velafit_config_init();
    }

  strncpy(g_config.wifi_ssid, ssid, sizeof(g_config.wifi_ssid) - 1);
  if (pwd != NULL)
    {
      strncpy(g_config.wifi_pwd, pwd, sizeof(g_config.wifi_pwd) - 1);
    }
  else
    {
      g_config.wifi_pwd[0] = '\0';
    }

  return velafit_config_save();
}

/****************************************************************************
 * Name: velafit_config_reset
 ****************************************************************************/

int velafit_config_reset(void)
{
  load_defaults();
  return velafit_config_save();
}

/****************************************************************************
 * Name: velafit_config_print
 ****************************************************************************/

void velafit_config_print(void)
{
  if (!g_config.initialized)
    {
      velafit_config_init();
    }

  char masked_key[64];
  size_t key_len = strlen(g_config.api_key);
  if (key_len == 0)
    {
      strncpy(masked_key, "(NOT SET - Mock Mode Active)",
              sizeof(masked_key) - 1);
    }
  else if (key_len <= 8)
    {
      strncpy(masked_key, "********", sizeof(masked_key) - 1);
    }
  else
    {
      snprintf(masked_key, sizeof(masked_key), "%.4s...%.4s (%zu chars)",
               g_config.api_key,
               g_config.api_key + key_len - 4,
               key_len);
    }

  printf("\n=======================================================\n");
  printf("  VelaFit AI Runtime & Cloud Configuration\n");
  printf("=======================================================\n");
  printf("  Base URL    : %s\n", g_config.base_url);
  printf("  API Key     : %s\n", masked_key);
  printf("  PRO Model   : %s\n", g_config.pro_model);
  printf("  ASR Model   : %s\n", g_config.asr_model);
  printf("  TTS Model   : %s\n", g_config.tts_model);
  printf("  Wi-Fi SSID  : %s\n",
         strlen(g_config.wifi_ssid) > 0 ? g_config.wifi_ssid : "(None)");
  printf("  Config File : %s\n", get_config_filepath());
  printf("=======================================================\n\n");
}
