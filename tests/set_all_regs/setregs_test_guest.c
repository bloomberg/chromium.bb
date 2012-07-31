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
#define CHECK_REG(reg_name) ASSERT_EQ(regs->reg_name, g_regs_req.reg_name)

#if defined(__i386__)
  CHECK_REG(eax);
  CHECK_REG(ecx);
  CHECK_REG(edx);
  CHECK_REG(ebx);
  CHECK_REG(stack_ptr);
  CHECK_REG(ebp);
  CHECK_REG(esi);
  CHECK_REG(edi);
  CHECK_REG(flags);
#elif defined(__x86_64__)
  CHECK_REG(rax);
  CHECK_REG(rbx);
  CHECK_REG(rcx);
  CHECK_REG(rdx);
  CHECK_REG(rsi);
  CHECK_REG(rdi);
  CHECK_REG(rbp);
  CHECK_REG(stack_ptr);
  CHECK_REG(r8);
  CHECK_REG(r9);
  CHECK_REG(r10);
  CHECK_REG(r11);
  CHECK_REG(r12);
  CHECK_REG(r13);
  CHECK_REG(r14);
  CHECK_REG(r15);
  CHECK_REG(flags);
#elif defined(__arm__)
  CHECK_REG(r0);
  CHECK_REG(r1);
  CHECK_REG(r2);
  CHECK_REG(r3);
  CHECK_REG(r4);
  CHECK_REG(r5);
  CHECK_REG(r6);
  CHECK_REG(r7);
  CHECK_REG(r8);
  CHECK_REG(r9);
  CHECK_REG(r10);
  CHECK_REG(r11);
  CHECK_REG(r12);
  CHECK_REG(stack_ptr);
  CHECK_REG(lr);
#else
# error Unsupported architecture
#endif

#undef CHECK_REG

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
#endif

  return 0;
}
