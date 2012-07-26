/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"


#if NACL_WINDOWS

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClGetCs):
        mov %cs, %eax
        ret

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClGetSs):
        mov %ss, %eax
        ret

#endif


        /* void NaClSwitchAllRegsAsm(struct NaClSwitchAllRegsState *state); */
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSwitchAllRegsAsm):
#if NACL_WINDOWS
        /* The first argument is in %rcx. */
        mov %rcx, %rsp
#elif NACL_LINUX || NACL_OSX
        /* The first argument is in %rdi. */
        mov %rdi, %rsp
#else
# error Unknown host OS
#endif
        /* Restore values from struct NaClSignalContext. */
        pop %rax
        pop %rbx
        pop %rcx
        pop %rdx
        pop %rsi
        pop %rdi
        pop %rbp
        addq $8, %rsp  /* Skip rsp value, which is restored by 'iretq' */
        pop %r8
        pop %r9
        pop %r10
        pop %r11
        pop %r12
        pop %r13
        pop %r14
        pop %r15
        /*
         * Skip over:  prog_ctr (8 bytes), flags, cs, ss, ds, es, fs,
         * gs and padding (4 bytes each).
         */
        addq $8 + 8*4, %rsp
        iretq
