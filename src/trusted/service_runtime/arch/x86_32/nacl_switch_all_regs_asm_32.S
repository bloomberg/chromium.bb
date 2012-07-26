/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"


        /* void NaClSwitchAllRegsAsm(struct NaClSwitchAllRegsState *state); */
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSwitchAllRegsAsm):
        /* Get function argument from stack. */
        movl 4(%esp), %ecx

        /* Restore general purpose registers except %ecx and %esp. */
        movl 0x18(%ecx), %eax
        movl 0x1c(%ecx), %edx
        movl 0x20(%ecx), %ebx
        movl 0x24(%ecx), %ebp
        movl 0x28(%ecx), %esi
        movl 0x2c(%ecx), %edi

        /* Restore flags. */
        leal 0x30(%ecx), %esp
        popf

        /* Restore segment registers to values for untrusted code. */
        movw 0x16(%ecx), %gs
        movw 0x14(%ecx), %fs
        movw 0x12(%ecx), %es

        /* Jump to springboard, restoring %cs. */
        ljmp *8(%ecx)
