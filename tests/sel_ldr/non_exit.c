/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

int main(int argc, char *argv[]) {
  struct timespec t = {1, 0};
  while (1) {
    nanosleep(&t, 0);
  }
  return 0;
}
