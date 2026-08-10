/****************************************************************************
 * board/contest_board/src/board_gpio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)

#ifndef CONFIG_ESPRESSIF_GPIO_IRQ
#  error "ESP32-P4X board GPIO requires CONFIG_ESPRESSIF_GPIO_IRQ"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <debug.h>
#include <nuttx/ioexpander/gpio.h>

#include <arch/board/board.h>
#include <arch/chip/gpio_sig_map.h>

#include "espressif/esp_gpio_p4.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct contest_gpio_dev_s
{
  struct gpio_dev_s gpio;
  uint8_t pin;
};

struct contest_gpioint_dev_s
{
  struct contest_gpio_dev_s gpio;
  pin_interrupt_t callback;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gpioout_read(struct gpio_dev_s *dev, bool *value);
static int gpioout_write(struct gpio_dev_s *dev, bool value);
static int gpioout_setpintype(struct gpio_dev_s *dev,
                              enum gpio_pintype_e pintype);
static int gpioin_read(struct gpio_dev_s *dev, bool *value);
static int gpioin_setpintype(struct gpio_dev_s *dev,
                             enum gpio_pintype_e pintype);
static int gpioint_read(struct gpio_dev_s *dev, bool *value);
static int gpioint_attach(struct gpio_dev_s *dev,
                          pin_interrupt_t callback);
static int gpioint_enable(struct gpio_dev_s *dev, bool enable);
static int gpioint_setpintype(struct gpio_dev_s *dev,
                              enum gpio_pintype_e pintype);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct gpio_operations_s g_gpioout_ops =
{
  .go_read       = gpioout_read,
  .go_write      = gpioout_write,
  .go_setpintype = gpioout_setpintype,
};

static const struct gpio_operations_s g_gpioin_ops =
{
  .go_read       = gpioin_read,
  .go_setpintype = gpioin_setpintype,
};

static const struct gpio_operations_s g_gpioint_ops =
{
  .go_read       = gpioint_read,
  .go_attach     = gpioint_attach,
  .go_enable     = gpioint_enable,
  .go_setpintype = gpioint_setpintype,
};

static struct contest_gpio_dev_s g_gpioout =
{
  .pin = BOARD_GPIO_OUT,
};

static struct contest_gpio_dev_s g_gpioin =
{
  .pin = BOARD_GPIO_IN,
};

static struct contest_gpioint_dev_s g_gpioint =
{
  .gpio.pin = BUTTON_BOOT,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int gpioout_read(struct gpio_dev_s *dev, bool *value)
{
  struct contest_gpio_dev_s *priv = (struct contest_gpio_dev_s *)dev;

  if (value == NULL)
    {
      return -EINVAL;
    }

  *value = esp_gpioread(priv->pin);
  return OK;
}

static int gpioout_write(struct gpio_dev_s *dev, bool value)
{
  struct contest_gpio_dev_s *priv = (struct contest_gpio_dev_s *)dev;

  esp_gpiowrite(priv->pin, value);
  return OK;
}

static int gpioout_setpintype(struct gpio_dev_s *dev,
                              enum gpio_pintype_e pintype)
{
  struct contest_gpio_dev_s *priv = (struct contest_gpio_dev_s *)dev;
  gpio_pinattr_t attr;

  switch (pintype)
    {
      case GPIO_INPUT_PIN:
        attr = INPUT;
        break;
      case GPIO_INPUT_PIN_PULLUP:
        attr = INPUT_PULLUP;
        break;
      case GPIO_INPUT_PIN_PULLDOWN:
        attr = INPUT_PULLDOWN;
        break;
      case GPIO_OUTPUT_PIN:
        attr = INPUT | OUTPUT;
        break;
      case GPIO_OUTPUT_PIN_OPENDRAIN:
        attr = INPUT | OUTPUT_OPEN_DRAIN;
        break;
      default:
        return -EINVAL;
    }

  esp_gpio_matrix_out(priv->pin, SIG_GPIO_OUT_IDX, false, false);
  return esp_configgpio(priv->pin, attr);
}

static int gpioin_read(struct gpio_dev_s *dev, bool *value)
{
  struct contest_gpio_dev_s *priv = (struct contest_gpio_dev_s *)dev;

  if (value == NULL)
    {
      return -EINVAL;
    }

  *value = esp_gpioread(priv->pin);
  return OK;
}

static int gpioin_setpintype(struct gpio_dev_s *dev,
                             enum gpio_pintype_e pintype)
{
  struct contest_gpio_dev_s *priv = (struct contest_gpio_dev_s *)dev;
  gpio_pinattr_t attr;

  switch (pintype)
    {
      case GPIO_INPUT_PIN:
        attr = INPUT;
        break;
      case GPIO_INPUT_PIN_PULLUP:
        attr = INPUT_PULLUP;
        break;
      case GPIO_INPUT_PIN_PULLDOWN:
        attr = INPUT_PULLDOWN;
        break;
      default:
        return -EINVAL;
    }

  return esp_configgpio(priv->pin, attr);
}

static int gpioint_handler(int irq, void *context, void *arg)
{
  struct contest_gpioint_dev_s *priv =
    (struct contest_gpioint_dev_s *)arg;

  if (priv->callback != NULL)
    {
      priv->callback(&priv->gpio.gpio, 0);
    }

  return OK;
}

static int gpioint_read(struct gpio_dev_s *dev, bool *value)
{
  struct contest_gpioint_dev_s *priv =
    (struct contest_gpioint_dev_s *)dev;

  if (value == NULL)
    {
      return -EINVAL;
    }

  *value = esp_gpioread(priv->gpio.pin);
  return OK;
}

static int gpioint_attach(struct gpio_dev_s *dev,
                          pin_interrupt_t callback)
{
  struct contest_gpioint_dev_s *priv =
    (struct contest_gpioint_dev_s *)dev;
  int ret;

  ret = esp_gpioirqdisable(priv->gpio.pin);
  if (ret < 0)
    {
      return ret;
    }

  if (priv->callback != NULL)
    {
      ret = esp_gpio_irq(priv->gpio.pin, NULL, NULL);
      if (ret < 0)
        {
          return ret;
        }

      priv->callback = NULL;
    }

  if (callback == NULL)
    {
      return OK;
    }

  priv->callback = callback;
  ret = esp_gpio_irq(priv->gpio.pin, gpioint_handler, priv);
  if (ret < 0)
    {
      priv->callback = NULL;
    }

  return ret;
}

static int gpioint_enable(struct gpio_dev_s *dev, bool enable)
{
  struct contest_gpioint_dev_s *priv =
    (struct contest_gpioint_dev_s *)dev;

  if (enable && priv->callback != NULL)
    {
      return esp_gpioirqenable(priv->gpio.pin);
    }

  return esp_gpioirqdisable(priv->gpio.pin);
}

static int gpioint_setpintype(struct gpio_dev_s *dev,
                              enum gpio_pintype_e pintype)
{
  struct contest_gpioint_dev_s *priv =
    (struct contest_gpioint_dev_s *)dev;
  gpio_pinattr_t attr;

  switch (pintype)
    {
      case GPIO_INTERRUPT_PIN:
      case GPIO_INTERRUPT_FALLING_PIN:
        attr = INPUT_PULLUP | FALLING;
        break;
      case GPIO_INTERRUPT_RISING_PIN:
        attr = INPUT_PULLDOWN | RISING;
        break;
      case GPIO_INTERRUPT_BOTH_PIN:
        attr = INPUT | CHANGE;
        break;
      case GPIO_INTERRUPT_HIGH_PIN:
        attr = INPUT_PULLDOWN | ONHIGH;
        break;
      case GPIO_INTERRUPT_LOW_PIN:
        attr = INPUT_PULLUP | ONLOW;
        break;
      default:
        return -EINVAL;
    }

  return esp_configgpio(priv->gpio.pin, attr);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int board_gpio_initialize(void)
{
  int ret;

  g_gpioout.gpio.gp_pintype = GPIO_OUTPUT_PIN;
  g_gpioout.gpio.gp_ops = &g_gpioout_ops;
  ret = gpio_pin_register(&g_gpioout.gpio, 0);
  if (ret < 0)
    {
      return ret;
    }

  esp_gpio_matrix_out(g_gpioout.pin, SIG_GPIO_OUT_IDX, false, false);
  esp_configgpio(g_gpioout.pin, INPUT | OUTPUT);
  esp_gpiowrite(g_gpioout.pin, false);

  g_gpioin.gpio.gp_pintype = GPIO_INPUT_PIN_PULLDOWN;
  g_gpioin.gpio.gp_ops = &g_gpioin_ops;
  ret = gpio_pin_register(&g_gpioin.gpio, 1);
  if (ret < 0)
    {
      return ret;
    }

  esp_configgpio(g_gpioin.pin, INPUT_PULLDOWN);

  g_gpioint.gpio.gpio.gp_pintype = GPIO_INTERRUPT_FALLING_PIN;
  g_gpioint.gpio.gpio.gp_ops = &g_gpioint_ops;
  ret = gpio_pin_register(&g_gpioint.gpio.gpio, 2);
  if (ret < 0)
    {
      return ret;
    }

  return esp_configgpio(g_gpioint.gpio.pin, INPUT_PULLUP | FALLING);
}

#endif
