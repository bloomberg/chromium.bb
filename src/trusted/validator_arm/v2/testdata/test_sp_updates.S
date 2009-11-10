@ Copyright 2009 The Native Client Authors.  All rights reserved.
@ Use of this source code is governed by a BSD-style license that can
@ be found in the LICENSE file.
@ Copyright 2009 Google Inc.

@
@ Tests both legal and illegal variations on SP updates.
@

#define MASK 0xF000000F

.globl _start
_start:
  .align 4

bundle0:
  ldr r0, [sp], #4          @ SP post-update is fine.
  ldr r0, [sp, #4]!         @ SP pre-update is fine.

  ldm sp!, { r0, r1, r2 }   @ Update from ldm is fine.
  stm sp!, { r0, r1, r2 }   @ Update from stm is fine.

bundle1:
  mov sp, r0                @ Move-to-SP is fine iff
  bic sp, sp, #MASK         @ immediately followed by mask.

  mov sp, r0                @ Move-to-SP not okay when followed by
  add r0, r0, #1            @ something unexpected (ERROR).

bundle2:
  add sp, sp, #12           @ Adding a constant is fine iff
  bic sp, sp, #MASK         @ immediately followed by mask.

  add sp, sp, #12           @ Not okay when followed by
  add sp, sp, #12           @ anything else (ERROR), or when...

bundle3:
  bic sp, sp, #MASK         @ ...broken across bundle (ERROR).

  nop
  nop
  mov sp, r0                @ Likewise, move-to-SP cannot be...

bundle4:
  bic sp, sp, #MASK         @ ...broken across bundle (ERROR).
