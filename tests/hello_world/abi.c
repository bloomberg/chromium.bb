/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Check ABI compliance, this is especially important for PNaCl
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/nacl_imc_api.h>
#include <sys/stat.h>
#include <sys/time.h>


int Assert(int actual, int expected, const char* mesg) {
  if (actual == expected) return 0;

  printf("ERROR: %s\n", mesg);
  printf("expected: %d, got: %d\n", expected, actual);

  return 1;
}

/* we expect this to be padded to 16 bytes of total size */
typedef struct {
  double d;
  float f;
} S1;


#define CHECK_SIZEOF(t, v) nerror += Assert(sizeof(t), v, "bad sizeof " #t)


int main(int argc, char* argv[]) {
  int nerror = 0;

  CHECK_SIZEOF(char, 1);
  CHECK_SIZEOF(short, 2);
  CHECK_SIZEOF(int, 4);
  CHECK_SIZEOF(long, 4);

  CHECK_SIZEOF(void*, 4);
  CHECK_SIZEOF(&main, 4);

  CHECK_SIZEOF(float, 4);
  CHECK_SIZEOF(double, 8);

  CHECK_SIZEOF(S1, 16);
  CHECK_SIZEOF(S1[2], 16*2);

  CHECK_SIZEOF(dev_t, 8);

  CHECK_SIZEOF(ino_t, 4);
  CHECK_SIZEOF(mode_t, 4);
  CHECK_SIZEOF(nlink_t, 4);
  CHECK_SIZEOF(uid_t, 4);
  CHECK_SIZEOF(gid_t, 4);
  CHECK_SIZEOF(off_t, 4);
  CHECK_SIZEOF(blksize_t, 4);
  CHECK_SIZEOF(blkcnt_t, 4);


  CHECK_SIZEOF(off_t, 4);
  CHECK_SIZEOF(size_t, 4);
  CHECK_SIZEOF(fpos_t, 4);

  CHECK_SIZEOF(time_t, 4);
  CHECK_SIZEOF(struct timezone, 8);
  CHECK_SIZEOF(suseconds_t, 4);
  CHECK_SIZEOF(clock_t, 4);
  CHECK_SIZEOF(struct timespec, 8);

  CHECK_SIZEOF(struct stat, 64);

  CHECK_SIZEOF(struct NaClImcMsgHdr, 20);

  return nerror;
}
