/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/common/register_set.h"


const uint8_t kX86FlagBits[5] = { 0, 2, 6, 7, 11 };

void RegsFillTestValues(struct NaClSignalContext *regs) {
  int index;
  for (index = 0; index < sizeof(*regs); index++) {
    ((char *) regs)[index] = index + 1;
  }
}

void RegsApplySandboxConstraints(struct NaClSignalContext *regs) {
#if defined(__x86_64__)
  uint64_t r15;
  __asm__("mov %%r15, %0" : "=r"(r15));
  regs->r15 = r15;
  regs->prog_ctr = r15 + (uint32_t) regs->prog_ctr;
  regs->stack_ptr = r15 + (uint32_t) regs->stack_ptr;
  regs->rbp = r15 + (uint32_t) regs->rbp;
#elif defined(__arm__)
  regs->r9 = (uintptr_t) nacl_tls_get();
#endif
}
