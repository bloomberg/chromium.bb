/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

.comm stack_ptr_at_crash,4,4

.global crash_at_known_address
.global prog_ctr_at_crash

        .p2align 4
crash_at_known_address:
        /*
         * Record the stack pointer first so that we can check that
         * the crash handler receives the same value.
         */
        /*
         * TODO(mcgrathr): The canonical syntax here would be #:lower16:...
         * but the LLVM assembler has a bug (that strikes on some uses of that
         * syntax and not others).  The syntax without the # is accepted by
         * both the GNU and LLVM assemblers.
         * http://code.google.com/p/nativeclient/issues/detail?id=2847
         */
        movw r0, :lower16:stack_ptr_at_crash
        movt r0, :upper16:stack_ptr_at_crash
        bic r0, r0, #0xc0000000
        str sp, [r0]
        /*
         * Second bundle:
         */
        mov r1, #0
        bic r1, r1, #0xc0000000
prog_ctr_at_crash:
        str r1, [r1]
