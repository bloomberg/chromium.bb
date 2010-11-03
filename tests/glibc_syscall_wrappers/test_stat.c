/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#pragma GCC diagnostic ignored "-Wnonnull"

#define KNOWN_FILE_SIZE 30

int main(int argc, char** argv) {
  struct stat st;
  struct stat64 st64;

  if (2 != argc) {
    printf("Usage: sel_ldr test_stat.nexe test_stat_data\n");
    return 1;
  }
  st.st_size = 0;
  st64.st_size = 0;

  assert(-1 == stat(NULL, &st));
  assert(EFAULT == errno);
  assert(-1 == stat(".", NULL));
  assert(EFAULT == errno);
  errno = 0;
  assert(0 == stat(argv[1], &st));
  assert(0 == errno);
  assert(KNOWN_FILE_SIZE == st.st_size);

  assert(-1 == stat64(NULL, &st64));
  assert(EFAULT == errno);
  assert(-1 == stat64(".", NULL));
  assert(EFAULT == errno);
  errno = 0;
  assert(0 == stat64(argv[1], &st64));
  assert(0 == errno);
  assert(KNOWN_FILE_SIZE == st64.st_size);

  return 0;
}
