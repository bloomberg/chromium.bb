/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>

#include "native_client/src/include/portability.h"

int loop_ffs(int v) {
  int rv = 1;
  int mask = 1;

  while (0 != mask) {
    if (v & mask) {
      return rv;
    }
    mask <<= 1;
    ++rv;
  }
  return 0;
}

/*
 * This exhaustively tests all 32-bit numbers, so will take a while
 * (about a minute on current machines).
 */
int TestFFS() {
  unsigned int errors = 0;
  uint32_t x;

  for (x = 1; 0 != x; ++x) {
    if (loop_ffs(x) != ffs(x)) {
      printf("ERROR: differs at %d (0x%x)\n", x, x);
      errors = 1;  /* if fail everywhere, errors would be UINT_MAX */
    }
  }
  if (loop_ffs(0) != ffs(0)) {
    printf("ERROR: differs at 0\n");
    errors = 1;
  }
  return errors;
}
