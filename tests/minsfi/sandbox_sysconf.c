/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  long result = sysconf(strtol(argv[1], NULL, 0));
  if (result != -1)
    return result;
  else
    return -errno;
}
