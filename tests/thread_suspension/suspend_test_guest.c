/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/thread_suspension/suspend_test.h"

#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/tests/common/register_set.h"


typedef int (*TYPE_nacl_test_syscall_1)(struct SuspendTestShm *test_shm);

__thread jmp_buf return_jmp_buf;


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
  struct NaClSignalContext *regs = &test_shm->expected_regs;
  char stack[0x10000];

  RegsFillTestValues(regs);
  regs->stack_ptr = (uintptr_t) stack + sizeof(stack);
  regs->prog_ctr = (uintptr_t) spin_instruction;
  RegsApplySandboxConstraints(regs);

  /*
   * Set registers to known test values and then spin.  We do not
   * block by entering a NaCl syscall because that would disturb the
   * register state.
   */
  test_shm->continue_after_suspension_func =
      (uintptr_t) ContinueAfterSuspension;
  assert(offsetof(struct SuspendTestShm, var) == 0);
#if defined(__i386__)
  regs->eax = (uintptr_t) test_shm;
  ASM_WITH_REGS(
      regs,
      /* Set "test_shm->var = test_shm" to indicate that we are ready. */
      "movl %%eax, (%%eax)\n"
      "spin_instruction:\n"
      "jmp spin_instruction\n");
#elif defined(__x86_64__)
  regs->rax = (uintptr_t) test_shm;
  ASM_WITH_REGS(
      regs,
      /* Set "test_shm->var = test_shm" to indicate that we are ready. */
      "movl %%eax, %%nacl:(%%r15, %%rax)\n"
      "spin_instruction:\n"
      "jmp spin_instruction\n");
#elif defined(__arm__)
  regs->r0 = (uintptr_t) test_shm;
  ASM_WITH_REGS(
      regs,
      /* Set "test_shm->var = test_shm" to indicate that we are ready. */
      "bic r0, r0, #0xc0000000\n"
      "str r0, [r0]\n"
      "spin_instruction:\n"
      "b spin_instruction\n");
#else
# error Unsupported architecture
#endif
  assert(!"Should not reach here");
}

static void ContinueAfterSyscall(void) {
  longjmp(return_jmp_buf, 1);
}

static void UnsetNonCalleeSavedRegisters(struct NaClSignalContext *regs) {
#if defined(__i386__)
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
#elif defined(__x86_64__)
  regs->rax = 0;
  regs->rcx = 0;
  regs->rdx = 0;
  regs->rsi = 0;
  regs->rdi = 0;
  regs->r8 = 0;
  regs->r9 = 0;
  regs->r10 = 0;
  regs->r11 = 0;
#elif defined(__arm__)
  regs->r0 = 0;
  regs->r1 = 0;
  regs->r2 = 0;
  regs->r3 = 0;
  regs->r12 = 0;
  regs->lr = 0;
#else
# error Unsupported architecture
#endif
}

/* Set registers to known values and enter a NaCl syscall. */
static void SyscallRegisterSetterThread(struct SuspendTestShm *test_shm) {
  struct NaClSignalContext call_regs;
  char stack[0x10000];

  RegsFillTestValues(&call_regs);
  call_regs.stack_ptr = (uintptr_t) stack + sizeof(stack);
  call_regs.prog_ctr = (uintptr_t) ContinueAfterSyscall;
  RegsApplySandboxConstraints(&call_regs);

  /*
   * call_regs are the registers we set on entry to the syscall.
   * expected_regs are the registers that should be reported by
   * NaClAppThreadGetSuspendedRegisters().  Since not all registers
   * are saved when entering a syscall, expected_regs will be the same
   * as call_regs but with various registers zeroed out.
   */
  test_shm->expected_regs = call_regs;
  UnsetNonCalleeSavedRegisters(&test_shm->expected_regs);

  uintptr_t syscall_addr = (uintptr_t) NACL_SYSCALL(test_syscall_1);
  if (!setjmp(return_jmp_buf)) {
#if defined(__i386__)
    test_shm->expected_regs.stack_ptr -= 4;  /* Account for argument */
    call_regs.eax = syscall_addr;
    call_regs.ecx = (uintptr_t) test_shm;  /* Scratch register */
    ASM_WITH_REGS(
        &call_regs,
        "push %%ecx\n"  /* Push syscall argument */
        "push $ContinueAfterSyscall\n"  /* Push return address */
        "nacljmp %%eax\n");
#elif defined(__x86_64__)
    call_regs.rax = syscall_addr;
    call_regs.rdi = (uintptr_t) test_shm;  /* Set syscall argument */
    ASM_WITH_REGS(
        &call_regs,
        "push $ContinueAfterSyscall\n"  /* Push return address */
        "nacljmp %%eax, %%r15\n");
#elif defined(__arm__)
    /*
     * We adjust for the 4 arguments that tramp_arm.S pushes on the
     * stack and arm/nacl_switch.S pops off the stack.
     *
     * TODO(mseaborn): We should change the TCB and remove this
     * adjustment, because it is wrong for
     * NaClAppThreadGetSuspendedRegisters() to report register state
     * that is different from what untrusted code will be resumed
     * with.
     */
    test_shm->expected_regs.stack_ptr -= 16;

    call_regs.r0 = (uintptr_t) test_shm;  /* Set syscall argument */
    call_regs.r1 = syscall_addr;  /* Scratch register */
    call_regs.lr = (uintptr_t) ContinueAfterSyscall;
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
  } else if (strcmp(test_type, "SyscallRegisterSetterThread") == 0) {
    SyscallRegisterSetterThread(test_shm);
  } else {
    fprintf(stderr, "Unknown test type: %s\n", test_type);
    return 1;
  }
  return 0;
}
