/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * More basic tests for floating point operations
 */

#include <stdio.h>
#include <stdint.h>

volatile uint32_t x32[] = {5, 6, 6000};
volatile uint64_t x64[] = {6, 6000, 5};

#define ARRAY_SIZE_UNSAFE(arr) ((sizeof arr)/sizeof arr[0])

int main(int argc, char* argv[]) {
  int i;
  for (i = 0; i < ARRAY_SIZE_UNSAFE(x32); ++i) {
    printf("%.6f\n", (double)x32[i]);
    printf("%.6f\n", (float)x32[i]);
  }

  for (i = 0; i < ARRAY_SIZE_UNSAFE(x64); ++i) {
    printf("%.6f\n", (double)x64[i]);
    printf("%.6f\n", (float)x64[i]);
  }

  return 0;
}
