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


typedef int (*TYPE_nacl_test_syscall_1)(struct NaClSignalContext *regs);

char g_stack[0x10000];
jmp_buf g_jmp_buf;
struct NaClSignalContext g_regs_req;

void SaveRegistersAndCheck();


#if defined(__i386__) || defined(__x86_64__)

/*
 * Normally it is possible to save x86 flags using the 'pushf'
 * instruction.  However, 'pushf' is disallowed under NaCl.  Instead,
 * we can read a subset of the flags indirectly using conditional
 * instructions.
 *
 * We save 5 flags:
 *   CF (carry), PF (parity), ZF (zero), SF (sign), OF (overflow)
 *
 * We don't attempt to check:
 *   AF (for BCD arithmetic only, which is not allowed by the validator)
 *   TF (trap flag:  not settable or observable by untrusted code)
 *   DF (direction flag:  indirectly observable, but it's a hassle to do so)
 */
static const uint8_t kX86FlagBits[5] = { 0, 2, 6, 7, 11 };

/* We use 'mov' and 'lea' because they do not modify the flags. */
#define SAVE_X86_FLAGS_INTO_REG(reg) \
    "mov $0, "reg"\n" \
    "jnc 0f; lea 1<<0("reg"), "reg"; 0:\n" \
    "jnp 0f; lea 1<<2("reg"), "reg"; 0:\n" \
    "jnz 0f; lea 1<<6("reg"), "reg"; 0:\n" \
    "jns 0f; lea 1<<7("reg"), "reg"; 0:\n" \
    "jno 0f; lea 1<<11("reg"), "reg"; 0:\n"

#endif


#if defined(__i386__)
__asm__(
    ".pushsection .text, \"ax\", @progbits\n"
    "SaveRegistersAndCheck:\n"
    /* Push most of "struct NaClSignalContext" in reverse order. */
    "push $0\n"  /* Leave space for flags */
    "push $0\n"  /* Leave space for prog_ctr */
    "push %edi\n"
    "push %esi\n"
    "push %ebp\n"
    "push %esp\n"
    "push %ebx\n"
    "push %edx\n"
    "push %ecx\n"
    "push %eax\n"
    /* Save flags. */
    SAVE_X86_FLAGS_INTO_REG("%eax")
    "movl %eax, 0x24(%esp)\n"
    /* Adjust saved %esp value to account for preceding pushes. */
    "addl $5 * 4, 0x10(%esp)\n"
    "push %esp\n"  /* Push argument to CheckSavedRegisters() function */
    "call CheckSavedRegisters\n"
    ".popsection\n");
#elif defined(__x86_64__)
__asm__(
    ".pushsection .text, \"ax\", @progbits\n"
    "SaveRegistersAndCheck:\n"
    /* Push most of "struct NaClSignalContext" in reverse order. */
    "push $0\n"  /* Leave space for flags */
    "push $0\n"  /* Leave space for prog_ctr */
    "push %r15\n"
    "push %r14\n"
    "push %r13\n"
    "push %r12\n"
    "push %r11\n"
    "push %r10\n"
    "push %r9\n"
    "push %r8\n"
    "push %rsp\n"
    "push %rbp\n"
    "push %rdi\n"
    "push %rsi\n"
    "push %rdx\n"
    "push %rcx\n"
    "push %rbx\n"
    "push %rax\n"
    /* Save flags. */
    SAVE_X86_FLAGS_INTO_REG("%rax")
    "movl %eax, 0x88(%rsp)\n"
    /* Adjust saved %rsp value to account for preceding pushes. */
    "addq $10 * 8, 0x38(%rsp)\n"
    "movl %esp, %edi\n"  /* Argument to CheckSavedRegisters() function */
    /* Align the stack pointer */
    "and $~15, %esp\n"
    "addq %r15, %rsp\n"
    "call CheckSavedRegisters\n"
    ".popsection\n");
#elif defined(__arm__)
__asm__(
    ".pushsection .text, \"ax\", %progbits\n"
    "SaveRegistersAndCheck:\n"
    "push {r14}\n"
    /*
     * "push {sp}" is undefined ("unpredictable") on ARM, so move sp
     * to a temporary register to push its original value.  (Indeed,
     * whether sp is modified before or after being written differs
     * between QEMU and the Panda boards.)
     */
    "add r14, sp, #4\n"
    "push {r0-r12, r14}\n"  /* Push most of "struct NaClSignalContext" */
    "mov r0, sp\n"  /* Argument to CheckSavedRegisters() function */
    "nop; nop; nop\n"  /* Padding to put the "bl" at the end of the bundle */
    "bl CheckSavedRegisters\n"
    ".popsection\n");
#else
# error Unsupported architecture
#endif

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
  /* Initialize the register set with arbitrary test data. */
  int index;
  for (index = 0; index < sizeof(g_regs_req); index++) {
    ((char *) &g_regs_req)[index] = index + 1;
  }

  g_regs_req.prog_ctr = (uintptr_t) SaveRegistersAndCheck;
  g_regs_req.stack_ptr = (uintptr_t) g_stack + sizeof(g_stack);

#if defined(__x86_64__)
  /* Adjust our expectations for the x86-64 sandbox's constraints. */
  uint64_t r15;
  __asm__("mov %%r15, %0" : "=r"(r15));
  g_regs_req.r15 = r15;
  g_regs_req.prog_ctr += r15;
  g_regs_req.stack_ptr += r15;
  g_regs_req.rbp = r15 + (uint32_t) g_regs_req.rbp;
#endif

#if defined(__i386__) || defined(__x86_64__)
  g_regs_req.flags = 0;
#endif

#if defined(__arm__)
  g_regs_req.r9 = (uintptr_t) nacl_tls_get();
#endif

  TestSettingRegisters();

#if defined(__i386__) || defined(__x86_64__)
  /* Test setting each x86 flag in turn. */
  for (index = 0; index < NACL_ARRAY_SIZE(kX86FlagBits); index++) {
    fprintf(stderr, "testing flags bit %i...\n", kX86FlagBits[index]);
    g_regs_req.flags = 1 << kX86FlagBits[index];
    TestSettingRegisters();
  }
#endif

  return 0;
}
