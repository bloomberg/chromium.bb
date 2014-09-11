/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

int main(int argc, char **argv) {
  int i, sum = 0;
  for (i = 0; i < argc; ++i)
    sum += strtol(argv[i], 0, 0);
  return sum;
}
