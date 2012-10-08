/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/tests/common/register_set.h"


/*
 * This test program calls a NaCl syscall in an infinite loop with
 * callee-saved registers set to known values.  These register values
 * are saved to a memory address for cross-checking by the
 * trusted-code portion of the test case.
 */

void SyscallLoop();
void SyscallReturnAddress();

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Expected 1 argument: <memory-address>\n");
    return 1;
  }

  char *end;
  struct NaClSignalContext *expected_regs =
      (struct NaClSignalContext *) strtoul(argv[1], &end, 0);
  assert(*end == '\0');

  struct NaClSignalContext call_regs;
  char stack[0x10000];

  RegsFillTestValues(&call_regs);
  call_regs.stack_ptr = (uintptr_t) stack + sizeof(stack);
  call_regs.prog_ctr = (uintptr_t) SyscallReturnAddress;
  RegsApplySandboxConstraints(&call_regs);
  RegsUnsetNonCalleeSavedRegisters(&call_regs);

  uintptr_t syscall_addr = NACL_SYSCALL_ADDR(NACL_sys_test_syscall_1);
#if defined(__i386__)
  call_regs.esi = syscall_addr;
  *expected_regs = call_regs;
  ASM_WITH_REGS(
      &call_regs,
      "SyscallLoop:\n"
      "naclcall %%esi\n"
      "SyscallReturnAddress:\n"
      "jmp SyscallLoop\n");
#elif defined(__x86_64__)
  call_regs.r12 = syscall_addr;
  *expected_regs = call_regs;
  ASM_WITH_REGS(
      &call_regs,
      "SyscallLoop:\n"
      /* Call via a temporary register so as not to modify %r12. */
      "mov %%r12d, %%eax\n"
      "naclcall %%eax, %%r15\n"
      "SyscallReturnAddress:\n"
      "jmp SyscallLoop\n");
#else
# error Unsupported architecture
#endif
}
