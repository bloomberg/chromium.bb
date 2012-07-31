/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_COMMON_REGISTER_SET_H_
#define NATIVE_CLIENT_TESTS_COMMON_REGISTER_SET_H_

#include "native_client/src/include/nacl_macros.h"
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
extern const uint8_t kX86FlagBits[5];

/* We use 'mov' and 'lea' because they do not modify the flags. */
# define SAVE_X86_FLAGS_INTO_REG(reg) \
    "mov $0, "reg"\n" \
    "jnc 0f; lea 1<<0("reg"), "reg"; 0:\n" \
    "jnp 0f; lea 1<<2("reg"), "reg"; 0:\n" \
    "jnz 0f; lea 1<<6("reg"), "reg"; 0:\n" \
    "jns 0f; lea 1<<7("reg"), "reg"; 0:\n" \
    "jno 0f; lea 1<<11("reg"), "reg"; 0:\n"

#endif

/*
 * REGS_SAVER_FUNC(def_func, callee_func) defines a function named
 * def_func which saves all registers on the stack and passes them to
 * callee_func in the form of a "struct NaClSignalContext *".
 */

#if defined(__i386__)

# define REGS_SAVER_FUNC(def_func, callee_func) \
    void def_func(); \
    void callee_func(struct NaClSignalContext *regs); \
    __asm__( \
        ".pushsection .text, \"ax\", @progbits\n" \
        #def_func ":\n" \
        /* Push most of "struct NaClSignalContext" in reverse order. */ \
        "push $0\n"  /* Leave space for flags */ \
        "push $0\n"  /* Leave space for prog_ctr */ \
        "push %edi\n" \
        "push %esi\n" \
        "push %ebp\n" \
        "push %esp\n" \
        "push %ebx\n" \
        "push %edx\n" \
        "push %ecx\n" \
        "push %eax\n" \
        /* Save flags. */ \
        SAVE_X86_FLAGS_INTO_REG("%eax") \
        "movl %eax, 0x24(%esp)\n" \
        /* Adjust saved %esp value to account for preceding pushes. */ \
        "addl $5 * 4, 0x10(%esp)\n" \
        /* Push argument to callee_func(). */ \
        "push %esp\n" \
        "call " #callee_func "\n" \
        ".popsection\n")

#elif defined(__x86_64__)

# define REGS_SAVER_FUNC(def_func, callee_func) \
    void def_func(); \
    void callee_func(struct NaClSignalContext *regs); \
    __asm__( \
        ".pushsection .text, \"ax\", @progbits\n" \
        #def_func ":\n" \
        /* Push most of "struct NaClSignalContext" in reverse order. */ \
        "push $0\n"  /* Leave space for flags */ \
        "push $0\n"  /* Leave space for prog_ctr */ \
        "push %r15\n" \
        "push %r14\n" \
        "push %r13\n" \
        "push %r12\n" \
        "push %r11\n" \
        "push %r10\n" \
        "push %r9\n" \
        "push %r8\n" \
        "push %rsp\n" \
        "push %rbp\n" \
        "push %rdi\n" \
        "push %rsi\n" \
        "push %rdx\n" \
        "push %rcx\n" \
        "push %rbx\n" \
        "push %rax\n" \
        /* Save flags. */ \
        SAVE_X86_FLAGS_INTO_REG("%rax") \
        "movl %eax, 0x88(%rsp)\n" \
        /* Adjust saved %rsp value to account for preceding pushes. */ \
        "addq $10 * 8, 0x38(%rsp)\n" \
        /* Set argument to callee_func(). */ \
        "movl %esp, %edi\n" \
        /* Align the stack pointer */ \
        "and $~15, %esp\n" \
        "addq %r15, %rsp\n" \
        "call " #callee_func "\n" \
        ".popsection\n")

#elif defined(__arm__)

/*
 * "push {sp}" is undefined ("unpredictable") on ARM, so we move sp to
 * a temporary register to push its original value.  (Indeed, whether
 * sp is modified before or after being written differs between QEMU
 * and the Panda boards.)
 */
# define REGS_SAVER_FUNC(def_func, callee_func) \
    void def_func(); \
    void callee_func(struct NaClSignalContext *regs); \
    __asm__( \
        ".pushsection .text, \"ax\", %progbits\n" \
        #def_func ":\n" \
        "push {r14}\n" \
        "add r14, sp, #4\n" \
        "push {r0-r12, r14}\n"  /* Push most of "struct NaClSignalContext" */ \
        "mov r0, sp\n"  /* Argument to callee_func() */ \
        /* Padding to put the "bl" at the end of the bundle */ \
        "nop; nop; nop\n" \
        "bl " #callee_func "\n" \
        ".popsection\n")

#else
# error Unsupported architecture
#endif

/* Initialize the register set with arbitrary test data. */
void RegsFillTestValues(struct NaClSignalContext *regs);

/* Adjust registers to follow the sandbox's constraints. */
void RegsApplySandboxConstraints(struct NaClSignalContext *regs);

#endif
