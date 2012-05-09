@ Copyright (c) 2011 The Native Client Authors. All rights reserved.
@ Use of this source code is governed by a BSD-style license that can be
@ found in the LICENSE file.

@
@ Tests whether we incorrectly recognize a pc-relative load as
@ a 2-instruction pattern, resulting in crosses-bundle errors if
@ the load instruction is the first instruction in a bundle.
@
@ Note: This test generates an error only if we incorrectly recognize
@ a pc-relative load as a 2-instruction pattern crossing bundle boundaries.
@

@ we restrict loads to the lower 1GB of the address space
#define MASK 0xc0000000

.globl _start
_start:
  .align 4

bundle0:
  ldr r0, [pc]   @ bundle 0 entry.
  nop
  nop
  nop
  ldr r0, [pc]   @ bundle 1 entry.
  b bundle0
  nop
  nop
  nop
