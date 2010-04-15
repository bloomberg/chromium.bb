/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

        .global template_func
        .global template_func_end
template_func:
        mov r0, #1234 & 0xff
        orr r0, #1234 & 0xff00
        bic lr, lr, #0xf000000f
        bx  lr
template_func_end:


        .global hlts
        .global hlts_end
        .p2align 4
hlts:
        bkpt 0x7777
        bkpt 0x7777
        bkpt 0x7777
        bkpt 0x7777
hlts_end:


        .global branch_forwards
        .global branch_forwards_end
        .global branch_backwards
        .global branch_backwards_end
        .p2align 4
branch_forwards:
        b branch_backwards
        .p2align 4
branch_forwards_end:
branch_backwards:
        b branch_forwards
        .p2align 4
branch_backwards_end:


       /* We include disallowed code below, so this must go into the
          data segment. */
        .data


        .global invalid_code
        .global invalid_code_end
        .p2align 4
invalid_code:
        bx lr
invalid_code_end:
