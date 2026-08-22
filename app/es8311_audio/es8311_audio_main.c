/****************************************************************************
 * app/es8311_audio/es8311_audio_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>
#include <nuttx/i2c/i2c_master.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ES8311_I2C_BUS      "/dev/i2c0"
#define ES8311_I2C_ADDR     0x18
#define ES8311_PCM_OUT      "/dev/audio/pcm0"
#define ES8311_PCM_IN       "/dev/audio/pcm_in0"

#define ES8311_REG_RESET    0x00
#define ES8311_REG_CLK_MGR1 0x01
#define ES8311_REG_ADC_DAC  0x14
#define ES8311_REG_CHIPID1  0xfd
#define ES8311_REG_CHIPID2  0xfe
#define ES8311_REG_CHIPID3  0xff

#define BUFFER_SAMPLES      1024
#define SAMPLE_RATE_DEFAULT 44100
#define CHANNELS_DEFAULT    2

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: es8311_i2c_read
 ****************************************************************************/

static int es8311_i2c_read(int fd, uint8_t reg, uint8_t *val)
{
  struct i2c_msg_s msg[2];
  struct i2c_transfer_s xfer;
  int ret;

  msg[0].frequency = 100000;
  msg[0].addr      = ES8311_I2C_ADDR;
  msg[0].flags     = 0;
  msg[0].buffer    = &reg;
  msg[0].length    = 1;

  msg[1].frequency = 100000;
  msg[1].addr      = ES8311_I2C_ADDR;
  msg[1].flags     = I2C_M_READ;
  msg[1].buffer    = val;
  msg[1].length    = 1;

  xfer.msgv = msg;
  xfer.msgc = 2;

  ret = ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&xfer);
  if (ret < 0)
    {
      return ret;
    }

  return OK;
}

/****************************************************************************
 * Name: do_probe
 ****************************************************************************/

static int do_probe(void)
{
  int fd;
  uint8_t id1 = 0;
  uint8_t id2 = 0;
  uint8_t id3 = 0;
  uint8_t reset = 0;
  uint8_t clk = 0;
  struct stat st;
  int ret;

  printf("[ES8311] Probing I2C bus: %s at addr 0x%02x\n",
         ES8311_I2C_BUS, ES8311_I2C_ADDR);

  fd = open(ES8311_I2C_BUS, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %d\n", ES8311_I2C_BUS, errno);
      return -errno;
    }

  ret = es8311_i2c_read(fd, ES8311_REG_CHIPID1, &id1);
  if (ret >= 0)
    {
      ret = es8311_i2c_read(fd, ES8311_REG_CHIPID2, &id2);
    }

  if (ret >= 0)
    {
      ret = es8311_i2c_read(fd, ES8311_REG_CHIPID3, &id3);
    }

  if (ret < 0)
    {
      printf("ERROR: I2C read failed from ES8311 (ret=%d, errno=%d)\n",
             ret, errno);
      close(fd);
      return ret;
    }

  es8311_i2c_read(fd, ES8311_REG_RESET, &reset);
  es8311_i2c_read(fd, ES8311_REG_CLK_MGR1, &clk);
  close(fd);

  printf("[ES8311] Chip ID: 0x%02X 0x%02X 0x%02X\n", id1, id2, id3);
  printf("[ES8311] REG00(Reset)=0x%02X, REG01(ClkMgr1)=0x%02X\n",
         reset, clk);

  if (id1 == 0x83 && id2 == 0x11)
    {
      printf("[ES8311] MATCH: Everest Semi ES8311 Codec Detected!\n");
    }
  else
    {
      printf("[ES8311] WARNING: Unexpected Chip ID!\n");
    }

  /* Check Audio Device Nodes */

  if (stat(ES8311_PCM_OUT, &st) == 0)
    {
      printf("[ES8311] Playback device node: %s [OK]\n", ES8311_PCM_OUT);
    }
  else
    {
      printf("[ES8311] Playback device node: %s [NOT FOUND]\n",
             ES8311_PCM_OUT);
    }

  if (stat(ES8311_PCM_IN, &st) == 0)
    {
      printf("[ES8311] Record device node:   %s [OK]\n", ES8311_PCM_IN);
    }
  else
    {
      printf("[ES8311] Record device node:   %s [NOT FOUND]\n",
             ES8311_PCM_IN);
    }

  return OK;
}

/****************************************************************************
 * Name: do_dump
 ****************************************************************************/

static int do_dump(void)
{
  int fd;
  uint8_t val;
  int i;
  int ret;

  fd = open(ES8311_I2C_BUS, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %d\n", ES8311_I2C_BUS, errno);
      return -errno;
    }

  printf("\n--- ES8311 Register Dump (0x00 .. 0x47) ---\n");
  for (i = 0; i <= 0x47; i++)
    {
      if (i % 16 == 0)
        {
          printf("\n%02X: ", i);
        }

      ret = es8311_i2c_read(fd, (uint8_t)i, &val);
      if (ret < 0)
        {
          printf("XX ");
        }
      else
        {
          printf("%02X ", val);
        }
    }

  printf("\n\n--- End of Dump ---\n");
  close(fd);
  return OK;
}

/****************************************************************************
 * Name: do_tone
 ****************************************************************************/

static int do_tone(int freq, int duration_sec)
{
  int fd;
  struct audio_caps_desc_s cap_desc;
  struct ap_buffer_info_s buf_info;
  struct ap_buffer_s *apb;
  struct audio_buf_desc_s buf_desc;
  int16_t *samples;
  int total_frames;
  int frames_sent = 0;
  int i;
  double phase = 0.0;
  double phase_inc;
  int ret;

  if (freq <= 0)
    {
      freq = 1000;
    }

  if (duration_sec <= 0)
    {
      duration_sec = 2;
    }

  printf("[ES8311] Sine: %d Hz for %d sec (44.1kHz 16-bit stereo)\n",
         freq, duration_sec);

  fd = open(ES8311_PCM_OUT, O_WRONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %d\n", ES8311_PCM_OUT, errno);
      return -errno;
    }

  /* 1. Configure audio format */

  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_OUTPUT;
  cap_desc.caps.ac_subtype        = AUDIO_TYPE_QUERY;
  cap_desc.caps.ac_channels       = CHANNELS_DEFAULT;
  cap_desc.caps.ac_controls.hw[0] = SAMPLE_RATE_DEFAULT;
  cap_desc.caps.ac_controls.b[2]  = 16; /* 16 bits */

  ret = ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);
  if (ret < 0)
    {
      printf("WARNING: AUDIOIOC_CONFIGURE returned %d\n", ret);
    }

  /* Set volume to 85% */

  cap_desc.caps.ac_subtype        = AUDIO_FU_VOLUME;
  cap_desc.caps.ac_controls.hw[0] = 850;
  ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);

  /* 2. Allocate buffer */

  memset(&buf_info, 0, sizeof(buf_info));
  buf_info.buffer_size = BUFFER_SAMPLES * CHANNELS_DEFAULT * sizeof(int16_t);
  buf_info.nbuffers    = 1;

  memset(&buf_desc, 0, sizeof(buf_desc));
  buf_desc.u.pbuffer   = &apb;

  ret = ioctl(fd, AUDIOIOC_ALLOCBUFFER, (unsigned long)&buf_desc);
  if (ret < 0 || apb == NULL)
    {
      printf("ERROR: AUDIOIOC_ALLOCBUFFER failed: %d\n", ret);
      close(fd);
      return ret;
    }

  /* 3. Start playback session */

  ret = ioctl(fd, AUDIOIOC_START, 0);
  if (ret < 0)
    {
      printf("WARNING: AUDIOIOC_START returned %d\n", ret);
    }

  total_frames = SAMPLE_RATE_DEFAULT * duration_sec;
  phase_inc = 2.0 * M_PI * (double)freq / (double)SAMPLE_RATE_DEFAULT;
  samples = (int16_t *)apb->samp;

  while (frames_sent < total_frames)
    {
      int chunk = BUFFER_SAMPLES;
      if (frames_sent + chunk > total_frames)
        {
          chunk = total_frames - frames_sent;
        }

      for (i = 0; i < chunk; i++)
        {
          int16_t sample = (int16_t)(sin(phase) * 20000.0);
          samples[i * 2]     = sample; /* Left */
          samples[i * 2 + 1] = sample; /* Right */
          phase += phase_inc;
          if (phase >= 2.0 * M_PI)
            {
              phase -= 2.0 * M_PI;
            }
        }

      apb->nbytes = chunk * CHANNELS_DEFAULT * sizeof(int16_t);
      buf_desc.u.buffer = apb;

      ret = ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, (unsigned long)&buf_desc);
      if (ret < 0)
        {
          printf("ERROR: Enqueue buffer failed: %d\n", ret);
          break;
        }

      frames_sent += chunk;
      usleep(chunk * 1000000UL / SAMPLE_RATE_DEFAULT);
    }

  /* 4. Stop and cleanup */

  ioctl(fd, AUDIOIOC_STOP, 0);
  ioctl(fd, AUDIOIOC_FREEBUFFER, (unsigned long)&buf_desc);
  close(fd);

  printf("[ES8311] Tone playback completed (%d frames played)\n",
         frames_sent);
  return OK;
}

/****************************************************************************
 * Name: do_record
 ****************************************************************************/

static int do_record(int duration_sec, FAR const char *filepath)
{
  int fd;
  int file_fd = -1;
  struct audio_caps_desc_s cap_desc;
  struct ap_buffer_s *apb = NULL;
  struct audio_buf_desc_s buf_desc;
  int total_frames;
  int frames_recorded = 0;
  int64_t sum_sq = 0;
  int total_samples = 0;
  int peak = 0;
  double rms;
  int ret;

  if (duration_sec <= 0)
    {
      duration_sec = 3;
    }

  printf("[ES8311] Recording from Microphone for %d seconds...\n",
         duration_sec);

  if (filepath != NULL)
    {
      file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (file_fd >= 0)
        {
          printf("[ES8311] Saving raw PCM to: %s\n", filepath);
        }
      else
        {
          printf("WARNING: Could not open %s for saving: %d\n",
                 filepath, errno);
        }
    }

  fd = open(ES8311_PCM_IN, O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %d\n", ES8311_PCM_IN, errno);
      if (file_fd >= 0)
        {
          close(file_fd);
        }

      return -errno;
    }

  /* 1. Configure recording format */

  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_INPUT;
  cap_desc.caps.ac_subtype        = AUDIO_TYPE_QUERY;
  cap_desc.caps.ac_channels       = CHANNELS_DEFAULT;
  cap_desc.caps.ac_controls.hw[0] = SAMPLE_RATE_DEFAULT;
  cap_desc.caps.ac_controls.b[2]  = 16;

  ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);

  /* 2. Allocate buffer */

  memset(&buf_desc, 0, sizeof(buf_desc));
  buf_desc.u.pbuffer = &apb;

  ret = ioctl(fd, AUDIOIOC_ALLOCBUFFER, (unsigned long)&buf_desc);
  if (ret < 0 || apb == NULL)
    {
      printf("ERROR: AUDIOIOC_ALLOCBUFFER failed for recording: %d\n", ret);
      close(fd);
      if (file_fd >= 0)
        {
          close(file_fd);
        }

      return ret;
    }

  /* 3. Start recording */

  ioctl(fd, AUDIOIOC_START, 0);
  total_frames = SAMPLE_RATE_DEFAULT * duration_sec;

  while (frames_recorded < total_frames)
    {
      int chunk = BUFFER_SAMPLES;
      int16_t *samples;
      int i;
      int n_samps;

      buf_desc.u.buffer = apb;
      ret = ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, (unsigned long)&buf_desc);
      if (ret < 0)
        {
          printf("ERROR: Receive buffer failed: %d\n", ret);
          break;
        }

      usleep(chunk * 1000000UL / SAMPLE_RATE_DEFAULT);

      samples = (int16_t *)apb->samp;
      n_samps = apb->nbytes / sizeof(int16_t);

      for (i = 0; i < n_samps; i++)
        {
          int val = abs((int)samples[i]);
          if (val > peak)
            {
              peak = val;
            }

          sum_sq += (int64_t)val * val;
        }

      total_samples += n_samps;
      frames_recorded += chunk;

      if (file_fd >= 0 && apb->nbytes > 0)
        {
          write(file_fd, apb->samp, apb->nbytes);
        }
    }

  ioctl(fd, AUDIOIOC_STOP, 0);
  ioctl(fd, AUDIOIOC_FREEBUFFER, (unsigned long)&buf_desc);
  close(fd);

  if (file_fd >= 0)
    {
      close(file_fd);
    }

  rms = (total_samples > 0) ? sqrt((double)sum_sq / total_samples) : 0.0;
  printf("[ES8311] Recorded frames: %d, Peak: %d, RMS: %.1f\n",
         frames_recorded, peak, rms);
  if (rms > 50.0)
    {
      printf("[ES8311] Microphone Input Signal Verified [OK]\n");
    }
  else
    {
      printf("[ES8311] Low signal level detected (check MIC bias/gain)\n");
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      printf("Usage: es8311_audio <command> [args]\n");
      printf("Commands:\n");
      printf("  probe                  Probe ES8311 chip and audio nodes\n");
      printf("  dump                   Dump ES8311 hardware registers\n");
      printf("  tone [freq] [sec]      Play tone (default 1000Hz 2s)\n");
      printf("  record [sec] [file]    Record PCM from mic (default 3s)\n");
      return 1;
    }

  if (strcmp(argv[1], "probe") == 0)
    {
      return do_probe();
    }
  else if (strcmp(argv[1], "dump") == 0)
    {
      return do_dump();
    }
  else if (strcmp(argv[1], "tone") == 0)
    {
      int freq = (argc > 2) ? atoi(argv[2]) : 1000;
      int sec  = (argc > 3) ? atoi(argv[3]) : 2;
      return do_tone(freq, sec);
    }
  else if (strcmp(argv[1], "record") == 0)
    {
      int sec = (argc > 2) ? atoi(argv[2]) : 3;
      FAR const char *f = (argc > 3) ? argv[3] : NULL;
      return do_record(sec, f);
    }
  else
    {
      printf("Unknown command: %s\n", argv[1]);
      return 1;
    }
}
