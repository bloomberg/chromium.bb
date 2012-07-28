/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_THREAD_SUSPENSION_REGISTER_SET_H_
#define NATIVE_CLIENT_TESTS_THREAD_SUSPENSION_REGISTER_SET_H_

#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"

/*
 * ASM_WITH_REGS(regs, asm_code) executes asm_code with most registers
 * restored from regs, a pointer of type "struct NaClSignalContext *".
 */

#if defined(__i386__)

# define ASM_WITH_REGS(regs, asm_code) \
    __asm__( \
        "movl %0, %%eax\n" \
        "movl 0x04(%%eax), %%ecx\n" \
        "movl 0x08(%%eax), %%edx\n" \
        "movl 0x0c(%%eax), %%ebx\n" \
        "movl 0x10(%%eax), %%esp\n" \
        "movl 0x14(%%eax), %%ebp\n" \
        "movl 0x18(%%eax), %%esi\n" \
        "movl 0x1c(%%eax), %%edi\n" \
        "movl 0x00(%%eax), %%eax\n" \
        asm_code \
        : : "r"(regs) : "memory")

#elif defined(__x86_64__)

# define ASM_WITH_REGS(regs, asm_code) \
    __asm__( \
        "naclrestbp %0, %%r15\n" \
        "movq 0x00(%%rbp), %%rax\n" \
        "movq 0x08(%%rbp), %%rbx\n" \
        "movq 0x10(%%rbp), %%rcx\n" \
        "movq 0x18(%%rbp), %%rdx\n" \
        "movq 0x20(%%rbp), %%rsi\n" \
        "movq 0x28(%%rbp), %%rdi\n" \
        /* Handle %rbp (0x30) later */ \
        "naclrestsp 0x38(%%rbp), %%r15\n" \
        "movq 0x40(%%rbp), %%r8\n" \
        "movq 0x48(%%rbp), %%r9\n" \
        "movq 0x50(%%rbp), %%r10\n" \
        "movq 0x58(%%rbp), %%r11\n" \
        "movq 0x60(%%rbp), %%r12\n" \
        "movq 0x68(%%rbp), %%r13\n" \
        "movq 0x70(%%rbp), %%r14\n" \
        "naclrestbp 0x30(%%rbp), %%r15\n" \
        asm_code \
        : : "r"(regs) : "memory")

#elif defined(__arm__)

/*
 * In principle we should be able to do just "ldmia r0, {r0-lr}", but:
 *   * We have to skip r9, since the validator currently makes it
 *     read-only (though service_runtime no longer trusts its
 *     content).
 *   * Oddly, PNaCl/Clang seems to have problems with '{' in inline
 *     assembly.
 *
 * Rather than debug Clang, I'm just writing out the register
 * restoration in the long form.
 *
 * We load values via sp since this requires no address masking.
 * Normally one would avoid this in case an async signal arrives while
 * sp doesn't point to a valid stack, but NaCl does not have async
 * signals at the moment, and this is just test code.
 */
# define ASM_WITH_REGS(regs, asm_code) \
    __asm__( \
        ".p2align 4\n" \
        "mov sp, %0\n" \
        "bic sp, sp, #0xc0000000\n" \
        "ldr r0, [sp, #0x00]\n" \
        "ldr r1, [sp, #0x04]\n" \
        "ldr r2, [sp, #0x08]\n" \
        "ldr r3, [sp, #0x0c]\n" \
        "ldr r4, [sp, #0x10]\n" \
        "ldr r5, [sp, #0x14]\n" \
        "ldr r6, [sp, #0x18]\n" \
        "ldr r7, [sp, #0x1c]\n" \
        "ldr r8, [sp, #0x20]\n" \
        /* Skip r9, since it is read-only */ \
        "ldr r10, [sp, #0x28]\n" \
        "ldr r11, [sp, #0x2c]\n" \
        "ldr r12, [sp, #0x30]\n" \
        "ldr lr, [sp, #0x38]\n" \
        /* Align to keep the following superinstruction within a bundle */ \
        ".p2align 4\n" \
        "ldr sp, [sp, #0x34]\n" \
        "bic sp, sp, #0xc0000000\n" \
        ".p2align 4\n"  /* Align for whatever comes after */ \
        asm_code \
        : : "r"(regs) : "memory")

#else
# error Unsupported architecture
#endif

/* Initialize the register set with arbitrary test data. */
static inline void RegsFillTestValues(struct NaClSignalContext *regs) {
  int index;
  for (index = 0; index < sizeof(*regs); index++) {
    ((char *) regs)[index] = index + 1;
  }
}

/* Adjust registers to follow the sandbox's constraints. */
static inline void RegsApplySandboxConstraints(struct NaClSignalContext *regs) {
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

#endif
