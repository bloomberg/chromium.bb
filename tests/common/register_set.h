/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_COMMON_REGISTER_SET_H_
#define NATIVE_CLIENT_TESTS_COMMON_REGISTER_SET_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_exception.h"

/*
 * ASM_WITH_REGS(regs, asm_code) executes asm_code with most registers
 * restored from regs, a pointer of type "struct NaClSignalContext *".
 */

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

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
        RESET_X86_FLAGS \
        asm_code \
        : : "r"(regs) : "memory")

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64

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
        RESET_X86_FLAGS \
        asm_code \
        : : "r"(regs) : "memory")

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

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
 */
# define REGS_MASK_R0 "bic r0, r0, #0xc0000000\n"
# define ASM_WITH_REGS(regs, asm_code) \
    __asm__( \
        ".p2align 4\n" \
        "mov r0, %0\n" \
        /* Set CPSR (flags) register, a.k.a. APSR for user mode */ \
        REGS_MASK_R0 "ldr r1, [r0, #0x40]\n" \
        "msr apsr_nzcvqg, r1\n" \
        /* Set stack pointer */ \
        REGS_MASK_R0 "ldr r1, [r0, #0x34]\n" \
        "bic sp, r1, #0xc0000000\n" \
        /* Ensure later superinstructions don't cross bundle boundaries */ \
        "nop\n" \
        /* Set general purpose registers */ \
        REGS_MASK_R0 "ldr r1, [r0, #0x04]\n" \
        REGS_MASK_R0 "ldr r2, [r0, #0x08]\n" \
        REGS_MASK_R0 "ldr r3, [r0, #0x0c]\n" \
        REGS_MASK_R0 "ldr r4, [r0, #0x10]\n" \
        REGS_MASK_R0 "ldr r5, [r0, #0x14]\n" \
        REGS_MASK_R0 "ldr r6, [r0, #0x18]\n" \
        REGS_MASK_R0 "ldr r7, [r0, #0x1c]\n" \
        REGS_MASK_R0 "ldr r8, [r0, #0x20]\n" \
        /* Skip r9, which is not supposed to be settable or readable */ \
        REGS_MASK_R0 "ldr r10, [r0, #0x28]\n" \
        REGS_MASK_R0 "ldr r11, [r0, #0x2c]\n" \
        REGS_MASK_R0 "ldr r12, [r0, #0x30]\n" \
        REGS_MASK_R0 "ldr lr, [r0, #0x38]\n" \
        /* Lastly, restore r0 */ \
        REGS_MASK_R0 "ldr r0, [r0, #0x00]\n" \
        ".p2align 4\n"  /* Align for whatever comes after */ \
        asm_code \
        : : "r"(regs) : "memory")

#else
# error Unsupported architecture
#endif

/*
 * JUMP_WITH_REGS(regs, dest) jumps to symbol |dest| with most
 * registers restored from |regs|, a pointer of type "struct
 * NaClSignalContext *".
 */
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
# define JUMP_WITH_REGS(regs, dest) ASM_WITH_REGS(regs, "jmp " #dest)
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
# define JUMP_WITH_REGS(regs, dest) ASM_WITH_REGS(regs, "b " #dest)
#else
# error Unsupported architecture
#endif

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

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

/* Reset flags to RESET_X86_FLAGS_VALUE without modifying any registers. */
#define RESET_X86_FLAGS "testl $0, %%eax\n"
#define RESET_X86_FLAGS_VALUE ((1 << 2) | (1 << 6))

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

/*
 * These are the only ARM CPSR bits that user code and untrusted code
 * can read and modify, excluding the IT bits which are for Thumb-2
 * (for If-Then-Else instructions).
 */
# define REGS_ARM_USER_CPSR_FLAGS_MASK \
    ((1<<31) | /* N */ \
     (1<<30) | /* Z */ \
     (1<<29) | /* C */ \
     (1<<28) | /* V */ \
     (1<<27) | /* Q */ \
     (1<<19) | (1<<18) | (1<<17) | (1<<16) /* GE bits */)

#endif

/*
 * REGS_SAVER_FUNC(def_func, callee_func) defines a function named
 * def_func which saves all registers on the stack and passes them to
 * callee_func in the form of a "struct NaClSignalContext *".
 */

#define REGS_SAVER_FUNC(def_func, callee_func) \
    void def_func(void); \
    REGS_SAVER_FUNC_NOPROTO(def_func, callee_func)

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

# define REGS_SAVER_FUNC_NOPROTO(def_func, callee_func) \
    void callee_func(struct NaClSignalContext *regs); \
    __asm__( \
        ".pushsection .text, \"ax\", @progbits\n" \
        ".p2align 5\n" \
        #def_func ":\n" \
        /* Push most of "struct NaClSignalContext" in reverse order. */ \
        "push $0\n"  /* Leave space for flags */ \
        "push $" #def_func "\n"  /* Fill out prog_ctr with known value */ \
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
        /* Save argument to callee_func() temporarily. */ \
        "mov %esp, %eax\n" \
        /* Align the stack pointer and leave space for an argument. */ \
        "pushl $0\n" \
        "and $~15, %esp\n" \
        /* Set argument to callee_func(). */ \
        "mov %eax, (%esp)\n" \
        "call " #callee_func "\n" \
        ".popsection\n")

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64

# define REGS_SAVER_FUNC_NOPROTO(def_func, callee_func) \
    void callee_func(struct NaClSignalContext *regs); \
    __asm__( \
        ".pushsection .text, \"ax\", @progbits\n" \
        ".p2align 5\n" \
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
        /* Fill out prog_ctr with known value */ \
        "leaq " #def_func "(%rip), %rax\n" \
        "movq %rax, 0x80(%rsp)\n" \
        /* Adjust saved %rsp value to account for preceding pushes. */ \
        "addq $10 * 8, 0x38(%rsp)\n" \
        /* Set argument to callee_func(). */ \
        "movl %esp, %edi\n" \
        /* Align the stack pointer */ \
        "and $~15, %esp\n" \
        "addq %r15, %rsp\n" \
        "call " #callee_func "\n" \
        ".popsection\n")

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

/*
 * "push {sp}" is undefined ("unpredictable") on ARM, so we move sp to
 * a temporary register to push its original value.  (Indeed, whether
 * sp is modified before or after being written differs between QEMU
 * and the Panda boards.)
 */
# define REGS_SAVER_FUNC_NOPROTO(def_func, callee_func) \
    void callee_func(struct NaClSignalContext *regs); \
    __asm__( \
        ".pushsection .text, \"ax\", %progbits\n" \
        ".p2align 4\n" \
        #def_func ":\n" \
        "push {r0}\n"  /* Leave space for cpsr */ \
        "push {r0}\n"  /* Leave space for prog_ctr */ \
        "push {r14}\n" \
        /* Save r0-r12 and sp; adjust sp for the pushes above */ \
        "add r14, sp, #0xc\n" \
        "push {r10-r12, r14}\n" \
        /* Push a dummy value for r9, which the tests need not compare */ \
        "mov r10, #0\n" \
        "push {r10}\n" \
        /* Save the rest of struct NaClSignalContext */ \
        "push {r0-r8}\n" \
        /* Now save a correct prog_ctr value */ \
        "adr r0, " #def_func "\n" \
        "str r0, [sp, #0x3c]\n" \
        /* Save CPSR (flags) register, a.k.a. APSR for user mode */ \
        "mrs r0, apsr\n" \
        "str r0, [sp, #0x40]\n" \
        /* Set argument to callee_func() */ \
        "mov r0, sp\n" \
        /* Align the stack pointer */ \
        "bic sp, sp, #0xc000000f\n" \
        "b " #callee_func "\n" \
        ".popsection\n")

#else
# error Unsupported architecture
#endif

/* Initialize the register set with arbitrary test data. */
void RegsFillTestValues(struct NaClSignalContext *regs, int seed);

/* Adjust registers to follow the sandbox's constraints. */
void RegsApplySandboxConstraints(struct NaClSignalContext *regs);

/* This compares for equality all registers saved by REGS_SAVER_FUNC. */
void RegsAssertEqual(const struct NaClSignalContext *actual,
                     const struct NaClSignalContext *expected);

/*
 * Copy a NaClUserRegisterState into a NaClSignalContext, leaving
 * trusted registers in the NaClSignalContext with unspecified values.
 */
void RegsCopyFromUserRegisterState(struct NaClSignalContext *dest,
                                   const NaClUserRegisterState *src);

/* Zero out registers that are clobbered by function calls. */
void RegsUnsetNonCalleeSavedRegisters(struct NaClSignalContext *regs);

/*
 * For a function called with register state |regs|, extract the first
 * argument.  This is useful for a function entry point defined by
 * REGS_SAVER_FUNC.
 */
uintptr_t RegsGetArg1(const struct NaClSignalContext *regs);

#endif
