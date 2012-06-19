/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


        .global template_func_misaligned_replacement
        .global template_func_misaligned_replacement_end
        .global template_func_illegal_call_target
        .global template_func_illegal_call_target_end
        .global template_func_illegal_register_replacement
        .global template_func_illegal_register_replacement_end
        .global template_func_illegal_guard_replacement
        .global template_func_illegal_guard_replacement_end
        .global template_func_illegal_constant_replacement
        .global template_func_illegal_constant_replacement_end
        .global template_func_external_jump_target
        .global template_func_external_jump_target_end
        .global template_func_external_jump_target_replace
        .global template_func_external_jump_target_replace_end
template_func_misaligned_replacement:
template_func_illegal_call_target:
template_func_illegal_register_replacement:
template_func_illegal_guard_replacement:
template_func_illegal_constant_replacement:
template_func_external_jump_target:
template_func_external_jump_target_replace:
        /* Not implemented */
        bkpt 0x7777   /* hlt */
template_func_misaligned_replacement_end:
template_func_illegal_call_target_end:
template_func_illegal_register_replacement_end:
template_func_illegal_guard_replacement_end:
template_func_illegal_constant_replacement_end:
template_func_external_jump_target_end:
template_func_external_jump_target_replace_end:

        .global template_func
        .global template_func_end
        .p2align 4
template_func:
        mov r0, #1234 & 0xff
        orr r0, #1234 & 0xff00
        bic lr, lr, #0xf000000f
        bx  lr
template_func_end:

        .global template_func_replacement
        .global template_func_replacement_end
        .p2align 4
template_func_replacement:
        mov r0, #4321 & 0xff
        orr r0, #4321 & 0xff00
        bic lr, lr, #0xf000000f
        bx  lr
template_func_replacement_end:

        .global template_func_nonreplacement
        .global template_func_nonreplacement_end
        .p2align 4
template_func_nonreplacement:
        mov r0, #1234 & 0xff
        orr r0, #1234 & 0xff00
        mov r1, #1234 & 0xff
        orr r1, #1234 & 0xff00
        bic lr, lr, #0xf000000f
        bx  lr
template_func_nonreplacement_end:

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


       /*
        * We include disallowed code below, so this must go into the
        * data segment.
        */
        .data


        .global invalid_code
        .global invalid_code_end
        .p2align 4
invalid_code:
        bx lr
invalid_code_end:
