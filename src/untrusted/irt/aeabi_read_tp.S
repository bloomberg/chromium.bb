/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

/*
 * The IRT uses its own private TLS, leaving r9 for the application.
 * We must define __aeabi_read_tp to get the private one instead of
 * the standard one.  The EABI requires that this function preserve
 * all registers except r0 (the return value register).  A normal
 * function is permitted to clobber r1, r2, and r3 as well, so we
 * cannot define this in C.
 */

        .globl __aeabi_read_tp
        .type __aeabi_read_tp,%function
        .p2align NACL_BLOCK_SHIFT
__aeabi_read_tp:
        push {r1, r2, r3, lr}
        nop
        nop
        /* NOTE: The call must be last slot in the bundle.
         *       Consider using the pnacl specifc pseudo opcodes
         *       once we build this with llvm-mc only.
         */
        bl __nacl_read_tp

        pop {r1, r2, r3, lr}
        bic lr, lr, #NACL_CONTROL_FLOW_MASK
        bx lr
