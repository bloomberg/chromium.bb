/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>

#include "libhello.h"

int main(int argc, char **argv) {
  int i;
  say_hello();
  for (i = 1; i < argc; i++) {
    printf("argv[%i] = %s\n", i, argv[i]);
  }
  return 0;
}
