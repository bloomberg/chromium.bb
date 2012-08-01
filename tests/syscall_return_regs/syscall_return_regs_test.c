/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/tests/common/register_set.h"


struct NaClSignalContext g_expected_regs;
jmp_buf g_return_jmp_buf;

REGS_SAVER_FUNC(ContinueAfterSyscall, CheckRegistersAfterSyscall);

void CheckRegistersAfterSyscall(struct NaClSignalContext *regs) {
  /* Ignore the register containing the return value. */
#if defined(__i386__)
  g_expected_regs.eax = regs->eax;
#elif defined(__x86_64__)
  g_expected_regs.rax = regs->rax;
#elif defined(__arm__)
  g_expected_regs.r0 = regs->r0;
#else
# error Unsupported architecture
#endif

  RegsAssertEqual(regs, &g_expected_regs);
  longjmp(g_return_jmp_buf, 1);
}

/* This tests a NaCl syscall that takes no arguments. */
void TestSyscall(uintptr_t syscall_addr) {
  struct NaClSignalContext call_regs;
  char stack[0x10000];

  RegsFillTestValues(&call_regs);
  call_regs.stack_ptr = (uintptr_t) stack + sizeof(stack);
  call_regs.prog_ctr = (uintptr_t) ContinueAfterSyscall;
  RegsApplySandboxConstraints(&call_regs);

  g_expected_regs = call_regs;
  RegsUnsetNonCalleeSavedRegisters(&g_expected_regs);

#if defined(__i386__) || defined(__x86_64__)
    /*
     * The context switch contains an "xorl %rN, %rN" to zero %rN
     * which also has the effect of resetting flags (at least those
     * recorded by REGS_SAVER_FUNC) to the following:
     *   bit 0: CF=0 (always set to 0 by xor)
     *   bit 2: PF=1 (bottom 8 bits of result have even number of 1s)
     *   bit 6: ZF=1 (set since result of xor is zero)
     *   bit 7: SF=0 (top bit of result)
     *   bit 11: OF=0 (always set to 0 by xor)
     */
  g_expected_regs.flags = (1 << 2) | (1 << 6);
#endif

  if (!setjmp(g_return_jmp_buf)) {
#if defined(__i386__)
    /* The current implementation sets %edx to the return address. */
    g_expected_regs.edx = (uintptr_t) ContinueAfterSyscall;

    call_regs.eax = syscall_addr;
    ASM_WITH_REGS(
        &call_regs,
        "push $ContinueAfterSyscall\n"  /* Push return address */
        "nacljmp %%eax\n");
#elif defined(__x86_64__)
    /* The current implementation sets %rcx to the return address. */
    g_expected_regs.rcx = call_regs.r15 + (uintptr_t) ContinueAfterSyscall;
    /* NaCl always sets %rdi (argument 1) as if it is calling _start. */
    g_expected_regs.rdi = (uintptr_t) call_regs.stack_ptr + 8;

    /*
     * This fast path syscall happens to preserve various registers,
     * but that is obviously not guaranteed by the ABI.
     */
    if (syscall_addr == (uintptr_t) NACL_SYSCALL(tls_get)) {
      g_expected_regs.rsi = call_regs.rsi;
      g_expected_regs.rdi = call_regs.rdi;
      g_expected_regs.r8 = call_regs.r8;
      g_expected_regs.r9 = call_regs.r9;
      g_expected_regs.r10 = call_regs.r10;
      g_expected_regs.r11 = call_regs.r11;
      g_expected_regs.r12 = call_regs.r12;
      g_expected_regs.r13 = call_regs.r13;
    }

    call_regs.rax = syscall_addr;
    ASM_WITH_REGS(
        &call_regs,
        "push $ContinueAfterSyscall\n"  /* Push return address */
        "nacljmp %%eax, %%r15\n");
#elif defined(__arm__)
    /* The current implementation sets r1 to the return address. */
    g_expected_regs.r1 = (uintptr_t) ContinueAfterSyscall;

    call_regs.r1 = syscall_addr;  /* Scratch register */
    call_regs.lr = (uintptr_t) ContinueAfterSyscall;  /* Return address */
    ASM_WITH_REGS(
        &call_regs,
        "bic r1, r1, #0xf000000f\n"
        "bx r1\n");
#else
# error Unsupported architecture
#endif
    assert(!"Should not reach here");
  }
}

int main() {
  printf("Testing null syscall...\n");
  TestSyscall((uintptr_t) NACL_SYSCALL(null));

  /*
   * Check tls_get specifically because it is implemented via a fast
   * path in assembly code.
   */
  printf("Testing tls_get syscall...\n");
  TestSyscall((uintptr_t) NACL_SYSCALL(tls_get));

  return 0;
}
