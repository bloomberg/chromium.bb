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
         * Clear the x87, MMX, and SSE state.
         * Then restore the untrusted code's x87 and SSE control words.
         * We could roll them together by storing a 512-byte per-thread
         * buffer and setting the control words in that in NaClSyscallSeg.
         * But that would bloat struct NaClThreadContext by 504 bytes or so,
         * and the performance cost of these two instructions after fxrstor
         * seems to be immeasurably small.
         */
        fxrstor fxrstor_default_state(%rip)
        fldcw   NACL_THREAD_CONTEXT_OFFSET_FCW(%rcx)
        ldmxcsr NACL_THREAD_CONTEXT_OFFSET_MXCSR(%rcx)

.if MACROARG2
        /*
         * Clear the AVX state that the "fxrstor" instruction doesn't cover.
         * We could roll them together by using the "xrstor" instruction, but
         * that has a complicated protocol and this seems to perform fine.
         *
         * This is "vzeroupper".
         * Some assembler versions don't know the AVX instructions.
         */
        .byte   0xc5, 0xf8, 0x77
.endif

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


        NACL_RODATA
        /*
         * This is the memory block for "fxrstor" to read.  The only
         * contents that matter are the fcw and mxcsr words, which we
         * store separately.  The mxcsr_mask word is ignored by the
         * hardware, so there is no need to get the hardware-supplied
         * value for that.  The hardware requires that this address be
         * aligned to 16 bytes.  Align it further to 64 bytes because
         * that is the usual size of a cache line; this might help
         * performance and is very unlikely to hurt it.
         */
        .balign 64
fxrstor_default_state:
        .space 512
