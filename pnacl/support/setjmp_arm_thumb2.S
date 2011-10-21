/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_arm_defs.h"

/*
 * jump_buf is organized as follows
 *
 * r4, r5, r6, r7, r8, r10 (sl), r11 (fp), r13 (sp), r14 (lr)
 *
 * note: we do not save/restore r9 - which pnacl uses for thread accounting
 */

    .text
    .align NACL_BLOCK_SHIFT
    .globl setjmp

/* int setjmp(jmp_buf env) */
setjmp:
    /* Bundle -------------------- */
    bic     r0, r0, #NACL_DATA_FLOW_MASK
    stmia   r0!, {r4, r5, r6, r7, r8, r10, r11, sp, lr}
    mov     r0, #0
    nop
    /* Bundle -------------------- */
    bic     lr, lr, #NACL_CONTROL_FLOW_MASK
    bx      lr


    .text
    .align NACL_BLOCK_SHIFT
    .globl longjmp

/* void longjmp(jmp_buf env, int val) */
longjmp:
    /* Bundle -------------------- */
    ldmia   r0!, {r4, r5, r6, r7, r8, r10, r11, r12, lr}
    mov     sp, r12
    bic     sp, sp, #NACL_DATA_FLOW_MASK
    /* Return the result argument if nonzero or 1 otherwise */
    /* Note: move updates condition codes */
    movs    r0, r1
    /* Bundle -------------------- */
    moveq   r0, #1
    bic     lr, lr, #NACL_CONTROL_FLOW_MASK
    bx      lr
