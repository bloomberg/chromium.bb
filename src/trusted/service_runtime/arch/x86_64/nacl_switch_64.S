/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This code gets executed when switching from the service
 * runtime to the NaCl module.  NaClSwitch has one parameter only,
 * which is a struct passed by reference.
 *
 * NB: this is not the only place where context switches from trusted
 * to untrusted code.  The NaClGetTlsFastPath* code (nacl_syscall_64.S)
 * also performs a partial switch (to and fro).
 */

#include "native_client/src/trusted/service_runtime/arch/x86_64/sel_rt_64.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

        /*
         * This function does not return.  Thus, we need to preserve
         * any callee-saved registers.
         */

        .text
/*
 * The MacOS assembler has a macro facility that is pretty close
 * to GNU as macros, but not quite the same.
 */
#if NACL_OSX || defined(__clang__)
# define MACRO(name)    .macro name
# define ENDMACRO       .endmacro
# define MACROENTRY     DEFINE_GLOBAL_HIDDEN_IDENTIFIER($0):
# define MACROARG1      $0
# define MACROARG2      $1
#else
# define MACRO(name)    .macro name arg1, arg2=0
# define ENDMACRO       .endm
# define MACROENTRY     DEFINE_GLOBAL_HIDDEN_IDENTIFIER(\arg1):
# define MACROARG1      \arg1
# define MACROARG2      \arg2
#endif


MACRO(switcher)
MACROENTRY
#if NACL_WINDOWS
        /* if Windows, 1st param is already in %rcx, not %rdi */
#elif NACL_LINUX || NACL_OSX
        /* elif Linux/OSX, 1st param is already in %rdi. */
        mov     %rdi, %rcx
#else
# error "What OS/compiler is the service runtime being compiled with?"
#endif

        movq    NACL_THREAD_CONTEXT_OFFSET_RBX(%rcx), %rbx
        movq    NACL_THREAD_CONTEXT_OFFSET_RBP(%rcx), %rbp
        movq    NACL_THREAD_CONTEXT_OFFSET_R12(%rcx), %r12
        movq    NACL_THREAD_CONTEXT_OFFSET_R13(%rcx), %r13
        movq    NACL_THREAD_CONTEXT_OFFSET_R14(%rcx), %r14
        movq    NACL_THREAD_CONTEXT_OFFSET_R15(%rcx), %r15

        /* there is no springboard for x86_64 */
        movq    NACL_THREAD_CONTEXT_OFFSET_RSP(%rcx), %rsp
        movq    NACL_THREAD_CONTEXT_OFFSET_SYSRET(%rcx), %rax

        /*
         * %rdi is the first argument in the user calling convention.
         * When starting the initial thread, we are passing the address
         * of the parameter block here.  The initial stack pointer has
         * been adjusted to one word below there, to insert a dummy
         * return address for the user entry point function.
         */
        leal    8(%rsp), %edi

        /*
         * Zero all unused registers.  The 32-bit instructions
         * are a byte shorter than their 64-bit counterparts
         * when the target register is one of the first eight,
         * and they implicitly zero the high halves.
         *
         * The 'xorl' instruction also resets most flags to known
         * values.
         */
        xorl    %edx, %edx
        movl    %edx, %esi
        movq    %rdx, %r8
        movq    %rdx, %r9
        movq    %rdx, %r10
        movq    %rdx, %r11

        /*
         * Clear the x87 state.  Then restore the untrusted code's control word.
         */
        fninit
        fldcw   NACL_THREAD_CONTEXT_OFFSET_FCW(%rcx)

        /*
         * Clear the vector registers.
         */
.if MACROARG2
        /*
         * This is "vzeroall".
         * Some assembler versions don't know the AVX instructions.
         */
        .byte   0xc5, 0xfc, 0x77
.else
        xorps   %xmm0, %xmm0
        xorps   %xmm1, %xmm1
        xorps   %xmm2, %xmm2
        xorps   %xmm3, %xmm3
        xorps   %xmm4, %xmm4
        xorps   %xmm5, %xmm5
        xorps   %xmm6, %xmm6
        xorps   %xmm7, %xmm7
        xorps   %xmm8, %xmm8
        xorps   %xmm9, %xmm9
        xorps   %xmm10, %xmm10
        xorps   %xmm11, %xmm11
        xorps   %xmm12, %xmm12
        xorps   %xmm13, %xmm13
        xorps   %xmm14, %xmm14
        xorps   %xmm15, %xmm15
.endif

        /*
         * Restore the untrusted code's SSE control word.
         */
        ldmxcsr NACL_THREAD_CONTEXT_OFFSET_MXCSR(%rcx)

        /*
         * Load the return address into %rcx rather than doing
         * "jmp *XXX(%rcx)" so that we do not leak the address of the
         * struct NaClThreadContext to untrusted code.  Knowing this
         * address would make bugs in the sandbox easier to exploit.
         */
        movq    NACL_THREAD_CONTEXT_OFFSET_NEW_PROG_CTR(%rcx), %rcx
        jmp     *%rcx
ENDMACRO

        switcher NaClSwitchSSE, 0
        switcher NaClSwitchAVX, 1
