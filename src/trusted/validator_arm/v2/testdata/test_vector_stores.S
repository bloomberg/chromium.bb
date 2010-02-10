@ Copyright 2010 The Native Client Authors.  All rights reserved.
@ Use of this source code is governed by a BSD-style license that can
@ be found in the LICENSE file.
@ Copyright 2010 Google Inc.

@
@ Tests masked and unmasked forms of vector stores.  Regression test for
@ http://code.google.com/p/nativeclient/issues/detail?id=270
@

.globl _start
_start:
  vst2.16 {d10[2],d12[2]}, [r0]     @ Unmasked, should fail.

  bic r0, r0, #0xC0000000           @ r0 is masked here, so
  vst1.16 {d10-d12}, [r0]           @ this should succeed.

  nop


  bic r0, r0, #0xC0000000           @ r0 is masked here, so
  vst1.8 {d0}, [r0, :64], r1        @ register postincrement should succeed

  bic r0, r0, #0xC0000000           @ r0 is masked here, but
  vst1.8 {d0}, [sp, :64], r1        @ SP+register should fail.
  nop
