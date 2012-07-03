/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * Compute the greatest common divisor using the Euclidean algorithm.
 */
static uintptr_t gcd_addr(uintptr_t x,
                          uintptr_t y) {
  uintptr_t tmp;

  if (x > y) {
    tmp = x;
    x = y;
    y = tmp;
  }
  /* x <= y */
  while (0 != x) {
    tmp = y % x;
    /* tmp < x */
    y = x;
    x = tmp;
  }
  return y;
}

static void tiny(void) {
  return;
}

static int small(void) {
  return 42;
}

static int medium(int x) {
  printf("x=%d\n", x);
  return medium(x+1);
}

uintptr_t NaClBundleSize(void) {
  uintptr_t  bundle_size;

  bundle_size = gcd_addr((uintptr_t) tiny, (uintptr_t) small);
  bundle_size = gcd_addr(bundle_size, (uintptr_t) medium);
  bundle_size = gcd_addr(bundle_size, (uintptr_t) gcd_addr);

  return bundle_size;
}

int main(void) {
  uintptr_t bundle_size = NaClBundleSize();
  printf("Bundle size is %d\n", bundle_size);
  if (0 != (bundle_size & (bundle_size - 1))) {
    printf("Not a power of 2\n");
    printf("FAILED\n");
    return 1;
  }
  printf("PASSED\n");
  return 0;
}
