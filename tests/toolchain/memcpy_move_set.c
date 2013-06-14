/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Simple test to verify that memcpy, memmove and memset are found and
 * work properly.
 */
#include <stdio.h>
#include <string.h>

/*
 * Array length and contents volatile to avoid constant folding.
 */
#define LENGTH 64
volatile unsigned length = LENGTH;
volatile unsigned char arr[LENGTH] = {3, 4, 5};

int main(void) {
  unsigned char *arrptr = (unsigned char *)arr;
  void *dst = memset(arrptr + 3, 6, length - 3);
  if (dst != arrptr + 3) {
    puts("Wrong return value from memset");
    return 1;
  }
  /* Now arr is [3, 4, 5, 6, 6, 6....] */
  printf("%u\n", (unsigned)arr[33]);  /* 6 */

  dst = memcpy(arrptr + length / 2, arrptr, length / 2);
  if (dst != arrptr + length / 2) {
    puts("Wrong return value from memcpy");
    return 1;
  }
  /* Now arr is [3, 4, 5, 6, ... until the middle, then 3, 4, 5, 6, 6, ...] */
  printf("%u\n", (unsigned)arr[33]);   /* 4 */

  /* memmove an overlapping range */
  dst = memmove(arrptr + 1, arrptr + 2, length / 2);
  if (dst != arrptr + 1) {
    puts("Wrong return value from memmove");
    return 1;
  }
  /* Now arr is [3, 5, 6, ... then same repeat in the middle] */
  printf("%u\n", (unsigned)arr[1]);    /* 5 */
  return 0;
}

