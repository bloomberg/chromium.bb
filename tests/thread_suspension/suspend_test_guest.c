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

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


typedef int (*TYPE_nacl_test_syscall_1)(struct SuspendTestShm *test_shm);


static void MutatorThread(struct SuspendTestShm *test_shm) {
  uint32_t next_val = 0;
  while (!test_shm->should_exit) {
    test_shm->var = next_val++;
  }
}

static void SyscallReturnThread(struct SuspendTestShm *test_shm) {
  int rc = NACL_SYSCALL(test_syscall_1)(test_shm);
  assert(rc == 0);
  /* Set value to indicate that the syscall returned. */
  test_shm->var = 99;
}

static void SyscallInvokerThread(struct SuspendTestShm *test_shm) {
  uint32_t next_val = 0;
  while (!test_shm->should_exit) {
    NACL_SYSCALL(null)();
    test_shm->var = next_val++;
  }
}

void spin_instruction();
void ContinueAfterSuspension();

static void RegisterSetterThread(struct SuspendTestShm *test_shm) {
  /*
   * Set registers to known test values and then spin.  We do not
   * block by entering a NaCl syscall because that would disturb the
   * register state.
   */
  test_shm->continue_after_suspension_func =
      (uintptr_t) ContinueAfterSuspension;
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

#if defined(__i386__)

struct SavedRegisters {
  uint32_t regs[6];
};

const uint32_t kTestValueBase = 0x12340001;

__asm__(
    ".pushsection .text, \"ax\", @progbits\n"
    "ContinueAfterSuspension:\n"
    /* Push "struct SavedRegisters" in reverse order. */
    "push %edi\n"
    "push %esi\n"
    "push %ebx\n"
    "push %edx\n"
    "push %ecx\n"
    "push %eax\n"
    "push %esp\n"  /* Push argument to CheckSavedRegisters() function */
    "call CheckSavedRegisters\n"
    ".popsection\n");

#elif defined(__x86_64__)

struct SavedRegisters {
  uint64_t regs[13];
};

const uint64_t kTestValueBase = 0x1234567800000001;

__asm__(
    ".pushsection .text, \"ax\", @progbits\n"
    "ContinueAfterSuspension:\n"
    /* Push "struct SavedRegisters" in reverse order. */
    "push %r14\n"
    "push %r13\n"
    "push %r12\n"
    "push %r11\n"
    "push %r10\n"
    "push %r9\n"
    "push %r8\n"
    "push %rdi\n"
    "push %rsi\n"
    "push %rbx\n"
    "push %rdx\n"
    "push %rcx\n"
    "push %rax\n"
    "movl %esp, %edi\n"  /* Argument to CheckSavedRegisters() function */
    /* Align the stack pointer */
    "and $~15, %esp\n"
    "addq %r15, %rsp\n"
    "call CheckSavedRegisters\n"
    ".popsection\n");

#elif defined(__arm__)

struct SavedRegisters {
  uint32_t regs[12];
};

const uint32_t kTestValueBase = 0x12340001;

__asm__(
    ".pushsection .text, \"ax\", %progbits\n"
    "ContinueAfterSuspension:\n"
    "push {r0-r8, r10-r12}\n"  /* Push "struct SavedRegisters" */
    "mov r0, sp\n"  /* Argument to CheckSavedRegisters() function */
    "nop\n"  /* Padding to put the "bl" at the end of the bundle */
    "bl CheckSavedRegisters\n"
    ".popsection\n");

#else
# error Unsupported architecture
#endif

void CheckSavedRegisters(struct SavedRegisters *saved_regs) {
  size_t index;
  for (index = 0; index < NACL_ARRAY_SIZE(saved_regs->regs); index++) {
    unsigned long long expected = kTestValueBase + index;
    unsigned long long actual = saved_regs->regs[index];
    if (actual != expected) {
      fprintf(stderr, "Failed: for register #%i, %llx != %llx\n",
              index, actual, expected);
      _exit(1);
    }
  }
  _exit(0);
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
  } else if (strcmp(test_type, "SyscallReturnThread") == 0) {
    SyscallReturnThread(test_shm);
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
