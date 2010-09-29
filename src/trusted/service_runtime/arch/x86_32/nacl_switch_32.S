/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
.globl  IDENTIFIER(NaClSwitch)
        HIDDEN(NaClSwitch)
IDENTIFIER(NaClSwitch):
        popl    %eax /* throw away the return addr */

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

        ljmp    *0x38(%ecx) /* springboard address */
