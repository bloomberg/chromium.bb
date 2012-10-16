/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#
# Tests unpredicted situations in NaCl for Mips.
#

.set JUMP_MASK,   $t6

.globl _start
.set noreorder
_start:

   b   _start - 0x10000        # OK - trampoline start.
    b   _start - 0x10000       # Error - branch is in the delay slot.
   nop
   nop

   and  $v0, $v0, JUMP_MASK
   jr   $v0                    # OK
    jr   $v0                   # Error - jump is in the delay slot.
   nop

   and  $v0, $v0, JUMP_MASK
   jr   $v0                    # OK
    b   _start - 0x10000       # Error - branch is in the delay slot.
   nop

   b   _start - 0x10000        # OK - trampoline start.
    jr   $v0                   # Error - jump is in the delay slot.
   nop
   nop

end_of_code:
