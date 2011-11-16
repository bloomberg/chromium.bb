/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  int64_t val = strtoll("-5", 0, 10);
  if (val == (int64_t) -5)
    return 0;
  printf("%lld != %lld\n", val, (int64_t) -5);
  return 1;
}
