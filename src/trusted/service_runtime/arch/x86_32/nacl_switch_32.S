/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Secure Runtime
 */

#include "native_client/src/trusted/service_runtime/arch/x86_32/sel_rt_32.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

/*
 * NB: why movl vs movw for moving to segment registers: movl is two
 * bytes, movw is three. ISA manual say these should be semantically
 * equivalent.  clang complains w/o explicit sizes.
 */

        .text

#define DO_NONE 0
#define DO_SSE 1
#define DO_AVX 2
#define DO_USING_ARG_IN_ECX 3

MACRO(switcher)
MACROENTRY
/* For DO_USING_ARG_IN_ECX don't touch the stack. */
.if MACROARG2 != DO_USING_ARG_IN_ECX
        popl    %eax /* throw away the return addr */
.endif

        /*
         * Clear the vector registers.
         */
.if MACROARG2 == DO_AVX
        /*
         * This is "vzeroall".
         * Some assembler versions don't know the AVX instructions.
         */
        .byte   0xc5, 0xfc, 0x77
.elseif MACROARG2 == DO_SSE
        xorps   %xmm0, %xmm0
        xorps   %xmm1, %xmm1
        xorps   %xmm2, %xmm2
        xorps   %xmm3, %xmm3
        xorps   %xmm4, %xmm4
        xorps   %xmm5, %xmm5
        xorps   %xmm6, %xmm6
        xorps   %xmm7, %xmm7
.endif

/*
 * For DO_USING_ARG_IN_ECX get the thread context from ecx instead of the
 * stack.
 */
.if MACROARG2 != DO_USING_ARG_IN_ECX
        /* do not leak info to app */
        xorl    %ecx, %ecx
        /* xorl will leave eflags in a known state, so no info leaks */
        popl    %ecx
.endif
        movl    NACL_THREAD_CONTEXT_OFFSET_NEW_PROG_CTR(%ecx), %edx
        movl    NACL_THREAD_CONTEXT_OFFSET_FRAME_PTR(%ecx), %ebp
        movl    NACL_THREAD_CONTEXT_OFFSET_EDI(%ecx), %edi
        movl    NACL_THREAD_CONTEXT_OFFSET_ESI(%ecx), %esi
        movl    NACL_THREAD_CONTEXT_OFFSET_EBX(%ecx), %ebx

        movw    NACL_THREAD_CONTEXT_OFFSET_GS(%ecx), %gs
        movw    NACL_THREAD_CONTEXT_OFFSET_FS(%ecx), %fs
        movw    NACL_THREAD_CONTEXT_OFFSET_ES(%ecx), %es

        /*
         * Clear the x87 state.  Then restore the untrusted code's control word.
         */
        fninit
        fldcw   NACL_THREAD_CONTEXT_OFFSET_FCW(%ecx)

.if MACROARG2 == DO_AVX || MACROARG2 == DO_SSE
        /*
         * Restore the untrusted code's SSE control word.
         */
        ldmxcsr NACL_THREAD_CONTEXT_OFFSET_MXCSR(%ecx)
.endif

        ljmp    *NACL_THREAD_CONTEXT_OFFSET_SPRING_ADDR(%ecx)
ENDMACRO

        switcher NaClSwitchNoSSE, DO_NONE
        switcher NaClSwitchSSE, DO_SSE
        switcher NaClSwitchAVX, DO_AVX

/* Generate register only version on OSX. */
#if NACL_OSX
        switcher NaClSwitchNoSSEViaECX, DO_USING_ARG_IN_ECX
#endif
