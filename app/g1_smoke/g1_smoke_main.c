/****************************************************************************
 * app/g1_smoke/g1_smoke_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define G1_DEFAULT_DURATION       10
#define G1_TIMER_PERIOD_NS        100000000L
#define G1_TIMER_HZ               10
#define G1_SIGNAL                 SIGUSR1
#define G1_BLOCK_COUNT            16
#define G1_REPORT_INTERVAL        60
#define G1_PERSIST_PATH           "/data/g1-persist.bin"
#define G1_PERSIST_MAGIC          UINT32_C(0x4731534d)
#define G1_PERSIST_VERSION        1
#define G1_PERSIST_PAYLOAD_SIZE   64
#define G1_ALLOWED_HEAP_DRIFT     4096

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct g1_persist_s
{
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint32_t checksum;
  uint8_t payload[G1_PERSIST_PAYLOAD_SIZE];
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t g1_checksum(const struct g1_persist_s *record)
{
  const uint8_t *data = (const uint8_t *)record;
  uint32_t checksum = UINT32_C(2166136261);
  size_t offset = offsetof(struct g1_persist_s, payload);
  size_t i;

  for (i = 0; i < sizeof(record->payload); i++)
    {
      checksum ^= data[offset + i];
      checksum *= UINT32_C(16777619);
    }

  checksum ^= record->magic;
  checksum *= UINT32_C(16777619);
  checksum ^= record->version;
  checksum *= UINT32_C(16777619);
  checksum ^= record->sequence;
  checksum *= UINT32_C(16777619);
  return checksum;
}

static int g1_persist_test(void)
{
  struct g1_persist_s record;
  struct g1_persist_s verify;
  ssize_t nread;
  ssize_t nwritten;
  uint32_t previous = 0;
  bool valid = false;
  int fd;
  int i;

  fd = open(G1_PERSIST_PATH, O_RDONLY);
  if (fd < 0)
    {
      if (errno != ENOENT)
        {
          printf("G1 FAIL storage open %s: %d\n", G1_PERSIST_PATH, errno);
          return -errno;
        }

      nread = 0;
    }
  else
    {
      memset(&record, 0, sizeof(record));
      nread = read(fd, &record, sizeof(record));
      close(fd);
    }

  if (nread == sizeof(record) && record.magic == G1_PERSIST_MAGIC &&
      record.version == G1_PERSIST_VERSION &&
      record.checksum == g1_checksum(&record))
    {
      previous = record.sequence;
      valid = true;
      printf("G1 PERSIST PASS sequence=%" PRIu32 "\n", previous);
    }
  else if (nread == 0)
    {
      printf("G1 PERSIST SEED no previous record\n");
    }
  else
    {
      printf("G1 FAIL invalid persistent record length=%ld\n", (long)nread);
      return -EIO;
    }

  memset(&record, 0, sizeof(record));
  record.magic = G1_PERSIST_MAGIC;
  record.version = G1_PERSIST_VERSION;
  record.sequence = valid ? previous + 1 : 1;
  for (i = 0; i < G1_PERSIST_PAYLOAD_SIZE; i++)
    {
      record.payload[i] = (uint8_t)(record.sequence + i * 37);
    }

  record.checksum = g1_checksum(&record);
  fd = open(G1_PERSIST_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      printf("G1 FAIL storage create %s: %d\n", G1_PERSIST_PATH, errno);
      return -errno;
    }

  nwritten = write(fd, &record, sizeof(record));
  if (nwritten != sizeof(record))
    {
      printf("G1 FAIL storage write length=%ld errno=%d\n",
             (long)nwritten, errno);
      close(fd);
      return -EIO;
    }

  if (fsync(fd) < 0)
    {
      printf("G1 FAIL storage fsync: %d\n", errno);
      close(fd);
      return -errno;
    }

  close(fd);

  fd = open(G1_PERSIST_PATH, O_RDONLY);
  if (fd < 0)
    {
      printf("G1 FAIL storage readback open: %d\n", errno);
      return -errno;
    }

  nread = read(fd, &verify, sizeof(verify));
  close(fd);
  if (nread != sizeof(verify) ||
      memcmp(&record, &verify, sizeof(record)) != 0)
    {
      printf("G1 FAIL storage immediate readback length=%ld\n", (long)nread);
      return -EIO;
    }

  printf("G1 PERSIST WRITE sequence=%" PRIu32 "\n", record.sequence);
  return 0;
}

static uint8_t g1_pattern(unsigned int iteration, int block, size_t offset)
{
  return (uint8_t)(iteration * 17u + (unsigned int)block * 31u + offset);
}

static int g1_heap_cycle(unsigned int iteration)
{
  void *blocks[G1_BLOCK_COUNT];
  size_t sizes[G1_BLOCK_COUNT];
  uint8_t *buffer;
  size_t i;
  size_t j;
  int block;

  memset(blocks, 0, sizeof(blocks));
  for (block = 0; block < G1_BLOCK_COUNT; block++)
    {
      sizes[block] = 257u +
                     ((iteration * 997u + (unsigned int)block * 4051u) %
                      65536u);
      blocks[block] = malloc(sizes[block]);
      if (blocks[block] == NULL)
        {
          printf("G1 FAIL heap allocation block=%d size=%lu\n", block,
                 (unsigned long)sizes[block]);
          goto fail;
        }

      buffer = blocks[block];
      for (i = 0; i < sizes[block]; i++)
        {
          buffer[i] = g1_pattern(iteration, block, i);
        }
    }

  for (block = G1_BLOCK_COUNT - 1; block >= 0; block--)
    {
      buffer = blocks[block];
      for (j = 0; j < sizes[block]; j++)
        {
          if (buffer[j] != g1_pattern(iteration, block, j))
            {
              printf("G1 FAIL heap data block=%d offset=%lu\n", block,
                     (unsigned long)j);
              goto fail;
            }
        }

      free(blocks[block]);
      blocks[block] = NULL;
    }

  return 0;

fail:
  for (block = 0; block < G1_BLOCK_COUNT; block++)
    {
      free(blocks[block]);
    }

  return -ENOMEM;
}

static int64_t g1_timespec_ms(const struct timespec *value)
{
  return (int64_t)value->tv_sec * 1000 + value->tv_nsec / 1000000;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct mallinfo before;
  struct mallinfo current;
  struct mallinfo after;
  struct sigevent event;
  struct itimerspec value;
  struct timespec wait;
  struct timespec started;
  struct timespec stopped;
  sigset_t signals;
  timer_t timerid;
  int64_t elapsed_ms;
  unsigned int expected;
  unsigned int iteration = 0;
  unsigned int report_at = G1_REPORT_INTERVAL * G1_TIMER_HZ;
  int duration = G1_DEFAULT_DURATION;
  int ret;

  if (argc > 1)
    {
      duration = atoi(argv[1]);
      if (duration <= 0 || duration > 24 * 60 * 60)
        {
          fprintf(stderr, "Usage: g1_smoke [duration-seconds]\n");
          return EXIT_FAILURE;
        }
    }

  printf("G1 START duration=%d timer_hz=%d\n", duration, G1_TIMER_HZ);
  ret = g1_persist_test();
  if (ret < 0)
    {
      return EXIT_FAILURE;
    }

  before = mallinfo();
  printf("G1 HEAP START arena=%d used=%d free=%d largest=%d\n",
         before.arena, before.uordblks, before.fordblks, before.mxordblk);

  sigemptyset(&signals);
  sigaddset(&signals, G1_SIGNAL);
  if (sigprocmask(SIG_BLOCK, &signals, NULL) < 0)
    {
      printf("G1 FAIL sigprocmask: %d\n", errno);
      return EXIT_FAILURE;
    }

  memset(&event, 0, sizeof(event));
  event.sigev_notify = SIGEV_SIGNAL;
  event.sigev_signo = G1_SIGNAL;
  if (timer_create(CLOCK_MONOTONIC, &event, &timerid) < 0)
    {
      printf("G1 FAIL timer_create: %d\n", errno);
      return EXIT_FAILURE;
    }

  memset(&value, 0, sizeof(value));
  value.it_value.tv_nsec = G1_TIMER_PERIOD_NS;
  value.it_interval.tv_nsec = G1_TIMER_PERIOD_NS;
  if (clock_gettime(CLOCK_MONOTONIC, &started) < 0 ||
      timer_settime(timerid, 0, &value, NULL) < 0)
    {
      printf("G1 FAIL timer start: %d\n", errno);
      timer_delete(timerid);
      return EXIT_FAILURE;
    }

  expected = (unsigned int)duration * G1_TIMER_HZ;
  wait.tv_sec = 1;
  wait.tv_nsec = 0;

  while (iteration < expected)
    {
      ret = sigtimedwait(&signals, NULL, &wait);
      if (ret != G1_SIGNAL)
        {
          printf("G1 FAIL timer signal iteration=%u ret=%d errno=%d\n",
                 iteration, ret, errno);
          timer_delete(timerid);
          return EXIT_FAILURE;
        }

      if (g1_heap_cycle(iteration) < 0)
        {
          timer_delete(timerid);
          return EXIT_FAILURE;
        }

      iteration++;
      if (iteration == report_at || iteration == expected)
        {
          current = mallinfo();
          printf("G1 STATUS elapsed=%u timer_signals=%u used=%d free=%d "
                 "largest=%d\n", iteration / G1_TIMER_HZ, iteration,
                 current.uordblks, current.fordblks, current.mxordblk);
          report_at += G1_REPORT_INTERVAL * G1_TIMER_HZ;
        }
    }

  memset(&value, 0, sizeof(value));
  timer_settime(timerid, 0, &value, NULL);
  timer_delete(timerid);
  clock_gettime(CLOCK_MONOTONIC, &stopped);
  elapsed_ms = g1_timespec_ms(&stopped) - g1_timespec_ms(&started);
  after = mallinfo();

  if (elapsed_ms < (int64_t)duration * 1000 - 1000 ||
      elapsed_ms > (int64_t)duration * 1000 + 2000)
    {
      printf("G1 FAIL monotonic elapsed=%" PRId64 "ms expected=%dms\n",
             elapsed_ms, duration * 1000);
      return EXIT_FAILURE;
    }

  if (after.uordblks > before.uordblks + G1_ALLOWED_HEAP_DRIFT)
    {
      printf("G1 FAIL heap drift before=%d after=%d limit=%d\n",
             before.uordblks, after.uordblks, G1_ALLOWED_HEAP_DRIFT);
      return EXIT_FAILURE;
    }

  printf("G1 TIMER PASS signals=%u elapsed=%" PRId64 "ms\n",
         iteration, elapsed_ms);
  printf("G1 HEAP PASS cycles=%u used_before=%d used_after=%d drift=%d\n",
         iteration, before.uordblks, after.uordblks,
         after.uordblks - before.uordblks);
  printf("G1 PASS\n");
  return EXIT_SUCCESS;
}
