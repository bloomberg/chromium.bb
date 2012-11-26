/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>
#include "native_client/tests/dynamic_code_loading/dynamic_segment.h"


/* Test that we can't write to the dynamic code area. */

int main(void) {
  void (*func)(void);
  uintptr_t code_ptr = (uintptr_t) DYNAMIC_CODE_SEGMENT_START;

  fprintf(stdout, "This should fault...\n");
  fflush(stdout);
#if defined(__i386__) || defined(__x86_64__)
  *(uint8_t *) code_ptr = 0xc3; /* RET */
#elif defined(__arm__)
  *(uint32_t *) code_ptr = 0xe12fff1e; /* BX LR */
#else
# error Unknown architecture
#endif

  fprintf(stdout, "We're still running. This is wrong.\n");
  fprintf(stdout, "Now try executing the code we wrote...\n");

  /* Double cast required to stop gcc complaining. */
  func = (void (*)(void)) (uintptr_t) code_ptr;
  func();
  fprintf(stdout, "We managed to run the code. This is bad.\n");
  return 1;
}
