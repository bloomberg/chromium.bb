/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/tests/common/register_set.h"
#include "native_client/tests/signal_handler_single_step/step_test_common.h"


/*
 * This test program calls a NaCl syscall in an infinite loop with
 * callee-saved registers set to known values.  These register values
 * are saved to a memory address for cross-checking by the
 * trusted-code portion of the test case.
 */

jmp_buf g_jmp_buf;
uint32_t g_regs_should_match;

#if defined(__i386__)
# define SYSCALL_CALLER(suffix) \
    __asm__("SyscallCaller" suffix ":\n" \
            "movl $1, g_regs_should_match\n" \
            "naclcall %esi\n" \
            "SyscallReturnAddress" suffix ":\n" \
            "movl $0, g_regs_should_match\n" \
            "jmp ReturnFromSyscall\n");
#elif defined(__x86_64__)
# define SYSCALL_CALLER(suffix) \
    __asm__("SyscallCaller" suffix ":\n" \
            "movl $1, g_regs_should_match(%rip)\n" \
            /* Call via a temporary register so as not to modify %r12. */ \
            "movl %r12d, %eax\n" \
            "naclcall %eax, %r15\n" \
            "SyscallReturnAddress" suffix ":\n" \
            "movl $0, g_regs_should_match(%rip)\n" \
            "jmp ReturnFromSyscall\n");
#else
# error Unsupported architecture
#endif

SYSCALL_CALLER("1")
SYSCALL_CALLER("2")
void SyscallCaller1(void);
void SyscallCaller2(void);
void SyscallReturnAddress1(void);
void SyscallReturnAddress2(void);

void ReturnFromSyscall(void) {
  longjmp(g_jmp_buf, 1);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Expected 1 argument: <memory-address>\n");
    return 1;
  }

  char *end;
  struct RegsTestShm *test_shm =
      (struct RegsTestShm *) strtoul(argv[1], &end, 0);
  assert(*end == '\0');
  test_shm->regs_should_match = &g_regs_should_match;

  struct NaClSignalContext call_regs;
  char stack[0x10000];

  int call_count = 0;
  for (call_count = 0; ; call_count++) {
    uintptr_t syscall_addr;
    /*
     * Test fast-path TLS syscalls.  We shoe-horn these in after the
     * first call to test_syscall_1 has enabled single-stepping.
     */
    if (call_count == 1) {
      syscall_addr = NACL_SYSCALL_ADDR(NACL_sys_tls_get);
    } else if (call_count == 2) {
      syscall_addr = NACL_SYSCALL_ADDR(NACL_sys_second_tls_get);
    } else {
      syscall_addr = NACL_SYSCALL_ADDR(NACL_sys_test_syscall_1);
    }

    /*
     * Use different expected register values for each call.
     * Otherwise, the test could accidentally pass because the
     * stack_ptr reported during the entry to a syscall can happen to
     * match the stack_ptr saved by the previous syscall.
     */
    RegsFillTestValues(&call_regs, /* seed= */ call_count);
#if defined(__i386__)
  call_regs.esi = syscall_addr;
#elif defined(__x86_64__)
  call_regs.r12 = syscall_addr;
#else
# error Unsupported architecture
#endif
    call_regs.prog_ctr = (uintptr_t) (call_count % 2 == 0
                                      ? SyscallReturnAddress1
                                      : SyscallReturnAddress2);
    call_regs.stack_ptr =
        (uintptr_t) stack + sizeof(stack) - (call_count % 2) * 0x100;
    RegsApplySandboxConstraints(&call_regs);
    RegsUnsetNonCalleeSavedRegisters(&call_regs);
    test_shm->expected_regs = call_regs;
    if (!setjmp(g_jmp_buf)) {
      if (call_count % 2 == 0) {
        JUMP_WITH_REGS(&call_regs, SyscallCaller1);
      } else {
        JUMP_WITH_REGS(&call_regs, SyscallCaller2);
      }
    }
  }
}
