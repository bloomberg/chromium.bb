/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/tests/common/register_set.h"


typedef int (*TYPE_nacl_test_syscall_1)(struct NaClSignalContext *regs);

char g_stack[0x10000];
jmp_buf g_jmp_buf;
struct NaClSignalContext g_regs_req;

REGS_SAVER_FUNC(SaveRegistersAndCheck, CheckSavedRegisters);

void CheckSavedRegisters(struct NaClSignalContext *regs) {
  RegsAssertEqual(regs, &g_regs_req);
  longjmp(g_jmp_buf, 1);
}


void TestSettingRegisters() {
  if (!setjmp(g_jmp_buf)) {
    NACL_SYSCALL(test_syscall_1)(&g_regs_req);
    assert(!"The syscall should not return");
  }
}


int main() {
  RegsFillTestValues(&g_regs_req);
  g_regs_req.prog_ctr = (uintptr_t) SaveRegistersAndCheck;
  g_regs_req.stack_ptr = (uintptr_t) g_stack + sizeof(g_stack);
  RegsApplySandboxConstraints(&g_regs_req);

#if defined(__i386__) || defined(__x86_64__)
  g_regs_req.flags = 0;
#elif defined(__arm__)
  g_regs_req.cpsr = 0;
#endif

  TestSettingRegisters();

#if defined(__i386__) || defined(__x86_64__)
  /* Test setting each x86 flag in turn. */
  int index;
  for (index = 0; index < NACL_ARRAY_SIZE(kX86FlagBits); index++) {
    fprintf(stderr, "testing flags bit %i...\n", kX86FlagBits[index]);
    g_regs_req.flags = 1 << kX86FlagBits[index];
    TestSettingRegisters();
  }
#elif defined(__arm__)
  /* Test setting each ARM CPSR flag in turn. */
  int bitshift;
  for (bitshift = 0; bitshift < 32; bitshift++) {
    if ((REGS_ARM_USER_CPSR_FLAGS_MASK & (1 << bitshift)) != 0) {
      fprintf(stderr, "testing CPSR bit %i...\n", bitshift);
      g_regs_req.cpsr = 1 << bitshift;
      TestSettingRegisters();
    }
  }
#endif

  return 0;
}
