/*
 * Copyright 2010, Google Inc.
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
 * Implementations of miscellaneous intrinsics.
 * See http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

    .text
    .section .text


/************************************************************************
 * __sync_val_compare_and_swap(int32* ptr,
 *                             int32 old_value,
 *                             int32 new_value)
 *
 * Atomic compare and swap.  If the current value of *ptr is old_value,
 * then write new_value into *ptr.  Returns previous value of *ptr.
 *
 * Args:
 *  r0 = ptr
 *  r1 = old_value
 *  r2 = new_value
 * Scratch:
 *  r3 = copy of ptr
 *  r12 = strex result
 * Returns:
 *  r0 = the contents of *ptr before the operation.
 */
    .align NACL_ARM_BUNDLE_SIZE_LOG
    .global __sync_val_compare_and_swap_4

__sync_val_compare_and_swap_4:
    mov r3, r0
.retryCAS:
    ldrex r0, [r3]
    cmp r1, r0
    bne .return

    bic r3, r3, #NACL_DATA_FLOW_MASK
    strex r12, r2, [r3]
    teq r12, #0
    /* Previous value of *ptr as expected, but another thread beat
     * us to the update.  No way to indicate failure, so retry. */
    bne .retryCAS
.return:
    bic lr, lr, #NACL_CONTROL_FLOW_MASK
    bx lr

/************************************************************************
 * __sync_fetch_and_add(int32* ptr, int32 increment)
 *
 * Atomically increments the memory word by the specified amount.
 *
 * Args:
 *  r0 = ptr
 *  r1 = increment
 * Scratch:
 *  r2 = exclusive store success?
 *  r3 = copy of ptr
 *  r12 = original
 * Returns:
 *  r0 = The previous value of the memory word.
 */
    .align NACL_ARM_BUNDLE_SIZE_LOG
    .global __sync_fetch_and_add_4

__sync_fetch_and_add_4:
    mov r3, r0
.retryFAA:
    ldrex r0, [r3]
    add r2, r0, r1
    nop

    bic r3, r3, #NACL_DATA_FLOW_MASK
    strex r12, r2, [r3]
    teq r12, #0
    bne .retryFAA /* try again */

    bic lr, lr, #NACL_CONTROL_FLOW_MASK
    bx lr
