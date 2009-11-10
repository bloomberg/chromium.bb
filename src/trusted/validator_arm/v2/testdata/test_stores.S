@ Copyright 2009 The Native Client Authors.  All rights reserved.
@ Use of this source code is governed by a BSD-style license that can
@ be found in the LICENSE file.
@ Copyright 2009 Google Inc.

@
@ Tests both legal and illegal variations on stores -- both stores that
@ require masking, and stores that are guaranteed sandboxed (i.e. through SP)
@

#define MASK 0xF0000000

.globl _start
_start:
  .align 4

bundle0:
  bic r1, r3, #MASK       @ Generating a confined address
  str r0, [r1]            @ and storing to it is fine.

  bic r1, r1, #MASK       @ Confining an address in place
  str r0, [r1]            @ and storing to it is fine.

bundle1:
  mov r1, r3              @ Just poking at the address
  str r0, [r1]            @ and storing to it is an ERROR.

  bic r1, r3, #0          @ Even if we use BIC, if the mask is wrong,
  str r0, [r1]            @ still an ERROR.

bundle2:
  nop
  nop
  nop
  bic r1, r3, #MASK       @ If the BIC is in a different bundle...

bundle3:
  str r0, [r1]            @ ...then the store is an ERROR.
