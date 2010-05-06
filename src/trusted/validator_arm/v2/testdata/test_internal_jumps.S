@ Copyright 2010 The Native Client Authors.  All rights reserved.
@ Use of this source code is governed by a BSD-style license that can
@ be found in the LICENSE file.

.global _start
_start:

@ Test forwards jumps
.p2align 4
  b bundle_good
  b middle_good
  b middle_bad

.p2align 4
bundle_good:
  nop
middle_good:
  bic lr, lr, #0xf000000f
middle_bad:
  bx lr

@ Test backwards jumps
.p2align 4
  b bundle_good
  b middle_good
  b middle_bad
