/****************************************************************************
 * board/contest_board/src/board_audio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_AUDIO_ES8311) && defined(CONFIG_ESP32P4_I2S0)

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <debug.h>

#include <nuttx/audio/audio.h>
#include <nuttx/audio/es8311.h>
#include <nuttx/audio/i2s.h>
#include <nuttx/i2c/i2c_master.h>

#include <arch/chip/esp32p4_i2s.h>
#include "espressif/esp_i2c.h"
#include "espressif/esp_gpio_p4.h"
#include "board_audio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ES8311_I2C_ADDR 0x18
#define ES8311_I2C_FREQ 100000
#define BOARD_PA_EN_PIN 53

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct es8311_lower_s g_es8311_tx_lower =
{
  .frequency = ES8311_I2C_FREQ,
  .address   = ES8311_I2C_ADDR,
};

#ifdef CONFIG_ESP32P4_I2S0_RX
static struct es8311_lower_s g_es8311_rx_lower =
{
  .frequency = ES8311_I2C_FREQ,
  .address   = ES8311_I2C_ADDR,
};
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_audio_initialize
 *
 * Description:
 *   Initialize the board audio subsystem (ES8311 + I2S0 + PA amplifier).
 *
 * Returned Value:
 *   OK on success; negated errno on failure.
 ****************************************************************************/

int board_audio_initialize(void)
{
  FAR struct audio_lowerhalf_s *es8311;
  FAR struct i2s_dev_s *i2s;
  FAR struct i2c_master_s *i2c;
  static bool initialized = false;
  int ret = OK;

  if (initialized)
    {
      return OK;
    }

  /* 1. Configure PA Enable GPIO53 (Active High) */

  esp_configgpio(BOARD_PA_EN_PIN, OUTPUT);
  esp_gpiowrite(BOARD_PA_EN_PIN, 1);

  /* 2. Initialize I2S0 */

  i2s = esp32p4_i2sbus_initialize(ESP32P4_I2S0);
  if (i2s == NULL)
    {
      auderr("ERROR: Failed to initialize I2S0\n");
      return -ENODEV;
    }

  /* 3. Initialize I2C0 */

  i2c = esp_i2cbus_initialize(ESPRESSIF_I2C0);
  if (i2c == NULL)
    {
      auderr("ERROR: Failed to initialize I2C0\n");
      return -ENODEV;
    }

  /* 4. Initialize and register ES8311 Playback Device /dev/audio/pcm0 */

#ifdef CONFIG_ESP32P4_I2S0_TX
  es8311 = es8311_initialize(i2c, i2s, &g_es8311_tx_lower);
  if (es8311 == NULL)
    {
      auderr("ERROR: Failed to initialize ES8311 TX\n");
      return -ENODEV;
    }

  ret = audio_register("pcm0", es8311);
  if (ret < 0)
    {
      auderr("ERROR: Failed to register /dev/audio/pcm0: %d\n", ret);
      return ret;
    }
#endif

  /* 5. Initialize and register ES8311 Recording Device /dev/audio/pcm_in0 */

#ifdef CONFIG_ESP32P4_I2S0_RX
  es8311 = es8311_initialize(i2c, i2s, &g_es8311_rx_lower);
  if (es8311 == NULL)
    {
      auderr("ERROR: Failed to initialize ES8311 RX\n");
      return -ENODEV;
    }

  ret = audio_register("pcm_in0", es8311);
  if (ret < 0)
    {
      auderr("ERROR: Failed to register /dev/audio/pcm_in0: %d\n", ret);
      return ret;
    }
#endif

  initialized = true;
  audinfo("ES8311 Audio subsystem initialized successfully\n");
  return OK;
}

#endif /* CONFIG_AUDIO_ES8311 && CONFIG_ESP32P4_I2S0 */
