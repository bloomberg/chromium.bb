# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# Test branch to addresses above 256 MB.
# We do this by linking _start at address 0xFFFA000 which is 255 MB+1000k,
# so that branch can reach address higher than 256 MB.

.globl _start
_start:
.align 4
.set noreorder

   nop
   b       _start + 0x10000     # Error, invalid destination, above 256MB.
   nop
   nop

end_of_code:
