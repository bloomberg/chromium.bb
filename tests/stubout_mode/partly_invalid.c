/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>

void unfixed_code(void);

#if defined(__i386__) || defined(__x86_64__)
__asm__("unfixed_code: ret\n");
#else
# error "Unsupported architecture"
#endif

int main() {
  printf("Before non-validating instruction\n");
  fflush(stdout);
  unfixed_code();
  printf("After non-validating instruction\n");
  return 0;
}
