/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *TODO(robertm): find documentation for eabi

 *                  The stack organization
 *                  0(sp)                   argc
 *                  4(sp)                   argv[0]
 *                  ...
 *                  (4*argc)(sp)            NULL
 *                  (4*(argc+1))(sp)        envp[0]
 *                  ...
 *                                          NULL
 */

/*  TODO(robertm): find a header to put this in/get it from, e.g. nacl_config */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

    .data
    .globl environ

    .text
    .align NACL_ARM_BUNDLE_SIZE_LOG

    .global _start

_start:
    mov r11, #0 /* clear frame pointer */
    mov r14, #0 /* clear link register */
    nop
    nop
    /* TODO(robertm): We should be doing this in sel_ldr. */
    ldr r0, [sp] /* get argc */
    add r1, sp, #4 /* get argv */
    add r2, r1, r0, lsl #2 /* get envp */
    add r2, r2, #4

    /* record the eviron pointer for getenv, etc. */
    movw r3, #:lower16:environ
    movt r3, #:upper16:environ
    bic r3, r3, #NACL_DATA_FLOW_MASK
    str r2, [r3]

    bic r3, sp, #7 /* aligned stack value in r3 */
    teq r3, sp  /* update sp if sp != r3, this will make it 8 aligned  */
    pushne {r1}
    bl __nacl_startup

    /* unreachable */
    bkpt    0x7777

/* ====================================================================== */
/* Some runtime code for ARM */
/* NOTE: this code can only clobber r0 and hence is hand coded */
/* Access to tls. We also have an unused trampoline version of this */
/* ====================================================================== */
    .align NACL_ARM_BUNDLE_SIZE_LOG
    .global __aeabi_read_tp

__aeabi_read_tp:
    /* the low order bits of r9 are for service runtime use only */
    mov r0, r9, lsr #NACL_PAGESHIFT
    lsl r0, #NACL_PAGESHIFT
    /* NOTE: for gcc/llvm generated code there is an implicit offset of 8/16 */
    /* TODO(robertm): find some flags to control/unify this */

    sub r0, r0, #16       /* llvm flavor */
   /* sub r0, r0, #8 */   /* gcc flavor */


    .align NACL_ARM_BUNDLE_SIZE_LOG
    bic lr, lr, #NACL_CONTROL_FLOW_MASK
    bx lr

