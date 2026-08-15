/****************************************************************************
 * app/sc2336_probe/sc2336_probe_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/i2c/i2c_master.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SC2336_DEVICE_PATH       "/dev/i2c0"
#define SC2336_SCCB_ADDRESS      0x30
#define SC2336_SCCB_FREQUENCY    100000
#define SC2336_CHIP_ID_HIGH      0x3107
#define SC2336_CHIP_ID_LOW       0x3108
#define SC2336_EXPECTED_ID       0xcb3a
#define SC2336_PROBE_RETRIES     5

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int sc2336_read_register(int fd, uint16_t regaddr, uint8_t *value)
{
  struct i2c_transfer_s transfer;
  struct i2c_msg_s messages[2];
  uint8_t address[2];

  address[0] = (uint8_t)(regaddr >> 8);
  address[1] = (uint8_t)regaddr;

  messages[0].frequency = SC2336_SCCB_FREQUENCY;
  messages[0].addr = SC2336_SCCB_ADDRESS;
  messages[0].flags = I2C_M_NOSTOP;
  messages[0].buffer = address;
  messages[0].length = sizeof(address);

  messages[1].frequency = SC2336_SCCB_FREQUENCY;
  messages[1].addr = SC2336_SCCB_ADDRESS;
  messages[1].flags = I2C_M_READ;
  messages[1].buffer = value;
  messages[1].length = 1;

  transfer.msgv = messages;
  transfer.msgc = 2;
  return ioctl(fd, I2CIOC_TRANSFER,
               (unsigned long)((uintptr_t)&transfer));
}

static int sc2336_read_retry(int fd, uint16_t regaddr, uint8_t *value)
{
  int ret = -EIO;
  int retry;

  for (retry = 1; retry <= SC2336_PROBE_RETRIES; retry++)
    {
      ret = sc2336_read_register(fd, regaddr, value);
      if (ret >= 0)
        {
          return 0;
        }

      printf("SC2336 probe retry=%d register=0x%04x ret=%d errno=%d\n",
             retry, regaddr, ret, errno);
      usleep(100000);
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  uint8_t high;
  uint8_t low;
  uint16_t chip_id;
  int fd;
  int ret;

  fd = open(SC2336_DEVICE_PATH, O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr, "SC2336 FAIL open %s: %d\n",
              SC2336_DEVICE_PATH, errno);
      return EXIT_FAILURE;
    }

  printf("SC2336 read-only probe address=0x%02x frequency=%d\n",
         SC2336_SCCB_ADDRESS, SC2336_SCCB_FREQUENCY);

  ret = sc2336_read_retry(fd, SC2336_CHIP_ID_HIGH, &high);
  if (ret < 0)
    {
      printf("SC2336 FAIL no response at address=0x%02x\n",
             SC2336_SCCB_ADDRESS);
      close(fd);
      return EXIT_FAILURE;
    }

  ret = sc2336_read_retry(fd, SC2336_CHIP_ID_LOW, &low);
  close(fd);
  if (ret < 0)
    {
      printf("SC2336 FAIL cannot read low ID register\n");
      return EXIT_FAILURE;
    }

  chip_id = ((uint16_t)high << 8) | low;
  if (chip_id != SC2336_EXPECTED_ID)
    {
      printf("SC2336 ID FAIL address=0x%02x id=0x%04x expected=0x%04x\n",
             SC2336_SCCB_ADDRESS, chip_id, SC2336_EXPECTED_ID);
      return EXIT_FAILURE;
    }

  printf("SC2336 ID PASS address=0x%02x id=0x%04x\n",
         SC2336_SCCB_ADDRESS, chip_id);
  return EXIT_SUCCESS;
}
