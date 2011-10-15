/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>

#define X86_FPCW_PREC_MASK      0x300
#define X86_FPCW_PREC_DOUBLE    0x200
#define X86_FPCW_PREC_EXTENDED  0x300

static uint16_t get_fpcw(void) {
  uint16_t fpcw;
  __asm__ __volatile__("fnstcw %0": "=m" (fpcw));
  return fpcw;
}

static void set_fpcw(uint16_t fpcw) {
  __asm__  __volatile__("fldcw %0": : "m" (fpcw));
}

int main(void) {
  uint16_t fpcw1 = 0;
  uint16_t fpcw2 = 0;

  /*
   * Change the FPU control word to a non-default setting.
   */
  fpcw1 = get_fpcw();
  assert(fpcw1 & X86_FPCW_PREC_EXTENDED);
  fpcw1 = (fpcw1 & ~X86_FPCW_PREC_MASK) | X86_FPCW_PREC_DOUBLE;
  set_fpcw(fpcw1);

  /*
   * Now do a system call.  It's supposed to clear the rest of the FPU
   * state, but leave the control word intact.
   */
  sched_yield();

  /*
   * Now check what we got.
   */
  fpcw2 = get_fpcw();
  if (fpcw2 != fpcw1) {
    fprintf(stderr, "Set FPCW to %hx and got back %hx\n", fpcw1, fpcw2);
    return 1;
  }

  return 0;
}
