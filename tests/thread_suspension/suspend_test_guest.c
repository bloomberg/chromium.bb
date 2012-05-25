/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/thread_suspension/suspend_test.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


static void MutatorThread(struct SuspendTestShm *test_shm) {
  uint32_t next_val = 0;
  while (!test_shm->should_exit) {
    test_shm->var = next_val++;
  }
}

static void SyscallInvokerThread(struct SuspendTestShm *test_shm) {
  uint32_t next_val = 0;
  while (!test_shm->should_exit) {
    NACL_SYSCALL(null)();
    test_shm->var = next_val++;
  }
}

void spin_instruction();

static void RegisterSetterThread(struct SuspendTestShm *test_shm) {
  /*
   * Set registers to known test values and then spin.  We do not
   * block by entering a NaCl syscall because that would disturb the
   * register state.
   */
  assert(offsetof(struct SuspendTestShm, var) == 0);
#if defined(__i386__)
  __asm__(
      /* Set %ecx = %esp + 0x1000 for testing %esp's value. */
      "movl %%esp, %%ecx\n"
      "addl $0x1000, %%ecx\n"
      /* Set simple test values. */
      "movl $0x10000001, %%edx\n"
      "movl $0x20000002, %%ebp\n"
      "movl $0x30000003, %%esi\n"
      "movl $0x40000004, %%edi\n"
      /* Set "test_shm->var = test_shm" to indicate that we are ready. */
      "movl %%eax, (%%eax)\n"
      "spin_instruction:\n"
      "jmp spin_instruction\n"
      : : "a"(test_shm) /* %eax */,
          "b"((uintptr_t) spin_instruction + 0x2000) /* %ebx */);
#elif defined(__x86_64__)
  __asm__(
      /* Set %rbx = spin_instruction + 0x2000 for testing %rip's value. */
      "leaq spin_instruction(%%rip), %%rbx\n"
      "addq $0x2000, %%rbx\n"
      /* Set %rcx = %rsp + 0x1000 for testing %rsp's value. */
      "movq %%rsp, %%rcx\n"
      "addq $0x1000, %%rcx\n"
      /* Set simple test values. */
      "movl $0x10000001, %%edx\n"
      "naclrestbp %%edx, %%r15\n"
      "movq $0x2000000000000002, %%rdx\n"
      "movq $0x3000000000000003, %%rsi\n"
      "movq $0x4000000000000004, %%rdi\n"
      "movq $0x8000000000000008, %%r8\n"
      "movq $0x9000000000000009, %%r9\n"
      "movq $0xa00000000000000a, %%r10\n"
      "movq $0xb00000000000000b, %%r11\n"
      "movq $0xc00000000000000c, %%r12\n"
      "movq $0xd00000000000000d, %%r13\n"
      "movq $0xe00000000000000e, %%r14\n"
      /* Set "test_shm->var = test_shm" to indicate that we are ready. */
      "movl %%eax, %%nacl:(%%r15, %%rax)\n"
      "spin_instruction:\n"
      "jmp spin_instruction\n"
      : : "a"(test_shm) /* %rax */);
#elif defined(__arm__)
  __asm__(
      "mov r0, %0\n"
      /* Set r1 = spin_instruction + 0x2000 for testing pc's value. */
      "adr r1, spin_instruction\n"
      "add r1, r1, #0x2000\n"
      /* Set r2 = sp + 0x1000 for testing sp's value. */
      "mov r2, sp\n"
      "add r2, r2, #0x1000\n"
      /* Set simple test values. */
      "mov r3, #0x30000003\n"
      "mov r4, #0x40000004\n"
      "mov r5, #0x50000005\n"
      "mov r6, #0x60000006\n"
      "mov r7, #0x70000007\n"
      "mov r8, #0x80000008\n"
      /* Skip r9, which is reserved for TLS and is read-only. */
      "mov r10, #0xa000000a\n"
      "mov r11, #0xb000000b\n"
      "mov r12, #0xc000000c\n"
      "mov lr, #0xe000000e\n"
      /* Set "test_shm->var = test_shm" to indicate that we are ready. */
      ".p2align 4\n"  /* Align for the following superinstruction. */
      "bic r0, r0, #0xc0000000\n"
      "str r0, [r0]\n"
      "spin_instruction:\n"
      "b spin_instruction\n"
      : : "r"(test_shm));
#else
# error Unsupported architecture
#endif
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Expected 2 arguments: <test-type> <memory-address>\n");
    return 1;
  }
  char *test_type = argv[1];

  char *end;
  struct SuspendTestShm *test_shm =
      (struct SuspendTestShm *) strtoul(argv[2], &end, 0);
  assert(*end == '\0');

  if (strcmp(test_type, "MutatorThread") == 0) {
    MutatorThread(test_shm);
  } else if (strcmp(test_type, "SyscallInvokerThread") == 0) {
    SyscallInvokerThread(test_shm);
  } else if (strcmp(test_type, "RegisterSetterThread") == 0) {
    RegisterSetterThread(test_shm);
  } else {
    fprintf(stderr, "Unknown test type: %s\n", test_type);
    return 1;
  }
  return 0;
}
