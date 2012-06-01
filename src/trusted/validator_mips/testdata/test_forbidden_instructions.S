# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

#
# Tests a few instructions that are always illegal in NaCl.
#

.globl _start
_start:

  eret               # Error, (forbidden) unsafe instruction.

  li        $v0, 1   # System call code for print_int.
  la        $a0, 20  # Integer for printing.
  syscall            # Error, this is forbidden in untrusted code.

  break              # Error, unsafe instruction.

  di                 # Error, unsafe instruction.
  ei                 # Error, unsafe instruction.
  wait               # Error, unsafe instruction.
  teq $t1, $t2       # Error, unsafe instruction.
  nop
  deret              # Error, unsafe instruction.

end_of_code:
