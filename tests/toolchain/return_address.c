/*
 * Copyright (c) 2011 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NOTE: because of fun pointer casting we need to disable -padantic. */
/* NOTE: because of aggressive inlining we need to disable -O2. */

#include <stdio.h>
#include <stdlib.h>
#include "native_client/tests/toolchain/utils.h"

int main(int argc, char* argv[]);

void recurse(int n, int is_first) {
  void* ra = (void*) __builtin_return_address (0);
  void* era = (void*) __builtin_extract_return_addr(ra);
  printf("ra: %p %p\n", ra, era);
  ASSERT(ra == era,
         "ERROR: until we know better we assert that ra == era\n");

  if (is_first) {
    ASSERT((void*) main < ra,
           "ERROR: ra to main is off\n");
  } else {
    ASSERT((void*) recurse < ra && ra < (void*) main,
           "ERROR: ra to recurse is off\n");
  }

  if (n == 0) return;
  recurse(n - 1, 0);
}

int main(int argc, char* argv[]) {
    /* NOTE: confuse optimizer, argc is never 5555 */
  if (argc != 5555 ) {
    argc = 10;
  }
  printf("main %p recurse %p\n", (void*) main, (void*) recurse);
  ASSERT((void*) recurse < (void*) main,
         "ERROR: this test assumes that main() follows recurse()\n");

  recurse(argc, 1);
  return 55;
}
