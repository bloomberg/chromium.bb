/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

.comm stack_ptr_at_crash,4,4

.global crash_at_known_address
.global prog_ctr_at_crash

crash_at_known_address:
        /*
         * Record the stack pointer so that we can check that the
         * crash handler receives the same value.
         */
        movl %esp, stack_ptr_at_crash
prog_ctr_at_crash:
        movl $0, 0
