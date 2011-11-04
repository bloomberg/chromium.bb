/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#define NUM_WORDS 512

int main(void) {
  void      *brk;
  uintptr_t brk_addr;
  int       *aligned;
  size_t    ix;
  int       status;

  fprintf(stderr, "sbrk test\n");
  brk = sbrk(0);
  brk_addr = (uintptr_t) brk;
  fprintf(stderr, "break at 0x%04x\n", brk_addr);
  if (0 != (brk_addr & 0x3)) {
    intptr_t incr = (sizeof aligned) - (brk_addr & ((sizeof aligned) - 1));

    fprintf(stderr, "break not word aligned, asking for %d more bytes\n",
            (int) incr);
    brk = sbrk(incr);
    if (NULL == brk) {
      fprintf(stderr, "sbrk for alignment failed\n");
      return 1;
    }
  }
  brk = sbrk(NUM_WORDS * sizeof *aligned);
  brk_addr = (uintptr_t) brk;
  fprintf(stderr, "allocated memory at 0x%04x\n", brk_addr);
  aligned = (int *) brk;

  for (ix = 0; ix < NUM_WORDS; ++ix) {
    aligned[ix] = 0xdeadbeef;
  }
  if ((void *) -1 == sbrk(-NUM_WORDS * (intptr_t) sizeof *aligned)) {
    fprintf(stderr, "freeing memory failed\n");
    return 2;
  }
  brk = sbrk(NUM_WORDS * sizeof *aligned);
  brk_addr = (uintptr_t) brk;
  fprintf(stderr, "break at 0x%04x\n", brk_addr);
  aligned = (int *) brk;
  status = 0;
  for (ix = 0; ix < NUM_WORDS; ++ix) {
    if (0 != aligned[ix]) {
      fprintf(stderr, "new memory word at %zd contains 0x%04x\n",
              ix, aligned[ix]);
      status = 3;
    }
  }
  return status;
}
