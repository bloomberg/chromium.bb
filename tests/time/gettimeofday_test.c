/*
 * Copyright 2009  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/nacl_syscalls.h>

/*
 * This mocking of gettimeofday is only useful for testing the libc
 * code, i.e., it's not very useful from the point of view of testing
 * Native Client.  To test the NaCl gettimeofday syscall
 * implementation, we need to match the "At the tone..." message and
 * do a approximate comparison with the output of python's
 * time.time().  I don't expect that we can actually require that they
 * match up to more than a few seconds, especialy for the heavily
 * loaded continuous build/test machines.
 */

time_t gMockTime = 0;

int mock_gettimeofday(struct timeval *tv, void *tz) {
  tv->tv_sec = gMockTime;
  tv->tv_usec = 0;
  return 0;
}

int (*timeofdayfn)(struct timeval *, void *) = gettimeofday;

int main(int ac,
         char **av) {
  int             opt;
  struct timeval  t_now;
  struct tm       t_broken_out;
  char            ctime_buf[26];
  char            *timestr;

  while (EOF != (opt = getopt(ac, av, "mt:"))) {
    switch (opt) {
      case 'm':
        timeofdayfn = mock_gettimeofday;
        break;
      case 't':
        gMockTime = strtol(optarg, (char **) NULL, 0);
        break;
      default:
        fprintf(stderr,
                "Usage: sel_ldr [sel_ldr-options]"
                " -- gettimeofday_test.nexe [-m]\n");
        return 1;
    }
  }
  if (0 != (*timeofdayfn)(&t_now, NULL)) {
    perror("gettimeofday_test: gettimeofday failed");
    printf("FAIL\n");
    return 2;
  }
  printf("At the tone, the system time is %ld.%06ld seconds.  BEEP!\n",
         t_now.tv_sec, t_now.tv_usec);
  localtime_r(&t_now.tv_sec, &t_broken_out);
  timestr = asctime_r(&t_broken_out, ctime_buf);
  if (NULL == timestr) {
    printf("FAIL\n");
  }
  printf("%s\n", timestr);
  return 0;
}
