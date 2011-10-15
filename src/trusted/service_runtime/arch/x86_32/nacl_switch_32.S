/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Secure Runtime
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

/*
 * NB: why movl vs movw for moving to segment registers: movl is two
 * bytes, movw is three. ISA manual say these should be semantically
 * equivalent.  clang complains w/o explicit sizes.
 */

        .text

/*
 * The MacOS assembler has a macro facility that is pretty close
 * to GNU as macros, but not quite the same.
 */
#if NACL_OSX || defined(__clang__)
# define MACRO(name)    .macro name
# define ENDMACRO       .endmacro
# define MACROENTRY     \
        .globl IDENTIFIER($0)   ; \
        HIDDEN($0)              ; \
        IDENTIFIER($0):
# define MACROARG1      $0
# define MACROARG2      $1
#else
# define MACRO(name)    .macro name arg1, arg2=0
# define ENDMACRO       .endm
# define MACROENTRY     \
        .globl IDENTIFIER(\arg1); \
        HIDDEN(\arg1)           ; \
        IDENTIFIER(\arg1):
# define MACROARG1      \arg1
# define MACROARG2      \arg2
#endif

/*
 * This is "vxorps %ymm\N, %ymm\N, %ymm\N".
 * Some assembler versions don't know the AVX instructions.
 */
MACRO(clear_ymm)
        .byte 0xc5
        .byte (~MACROARG1 << 3) | 4
        .byte 0x57
        .byte 0xc0 | MACROARG1 | (MACROARG1 << 3)
ENDMACRO

#define DO_NONE 0
#define DO_SSE 1
#define DO_AVX 2

MACRO(switcher)
MACROENTRY
        popl    %eax /* throw away the return addr */

        /*
         * Clear the vector registers.
         */
.if MACROARG2 == DO_AVX
        clear_ymm 0
        clear_ymm 1
        clear_ymm 2
        clear_ymm 3
        clear_ymm 4
        clear_ymm 5
        clear_ymm 6
        clear_ymm 7
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

        /* do not leak info to app */
        xorl    %ecx, %ecx
        /* xorl will leave eflags in a known state, so no info leaks */
        popl    %ecx
        movl    0x30(%ecx), %edx /* new_eip */
        movl    0x14(%ecx), %ebp /* frame_ptr.ptr_32.ptr */
        movl    0x8(%ecx), %edi /* edi */
        movl    0x4(%ecx), %esi /* esi */
        movl    0(%ecx), %ebx /* ebx */

        movw    0x2e(%ecx), %gs /* gs */
        movw    0x2c(%ecx), %fs /* fs */
        movw    0x2a(%ecx), %es /* es */

        /*
         * Clear the x87 state.  Then restore the untrusted code's control word.
         */
        fninit
        fldcw   0x22(%ecx)

        ljmp    *0x38(%ecx) /* springboard address */
ENDMACRO

        switcher NaClSwitchNoSSE, DO_NONE
        switcher NaClSwitchSSE, DO_SSE
        switcher NaClSwitchAVX, DO_AVX
