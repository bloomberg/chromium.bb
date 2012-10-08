/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_mips_defs.h"

  .text
  .p2align 4
  .align NACL_BLOCK_SHIFT
  .globl setjmp
  .ent setjmp
  .frame  $sp, 0, $31
  .set noreorder

/* int setjmp(jmp_buf env) */
setjmp:

  /* The code here is grouped by instruction bundle. */

  /* NACL_CALLEE_SAVE_LIST BEGIN */
  and  $a0, $a0, STORE_MASK
  sw   $s0, 0($a0)
  and  $a0, $a0, STORE_MASK
  sw   $s1, 4($a0)

  and  $a0, $a0, STORE_MASK
  sw   $s2, 8($a0)
  and  $a0, $a0, STORE_MASK
  sw   $s3, 12($a0)

  and  $a0, $a0, STORE_MASK
  sw   $s4, 16($a0)
  and  $a0, $a0, STORE_MASK
  sw   $s5, 20($a0)

  and  $a0, $a0, STORE_MASK
  sw   $s6, 24($a0)
  and  $a0, $a0, STORE_MASK
  sw   $s7, 28($a0)

  and  $a0, $a0, STORE_MASK
  sw   $sp, 32($a0)
  and  $a0, $a0, STORE_MASK
  sw   $fp, 36($a0)

  and  $a0, $a0, STORE_MASK
  sw   $ra, 40($a0)
  and  $a0, $a0, STORE_MASK
  swc1 $f20, 44($a0)

  and  $a0,  $a0, STORE_MASK
  swc1 $f21, 48($a0)
  and  $a0,  $a0, STORE_MASK
  swc1 $f22, 52($a0)

  and  $a0,  $a0, STORE_MASK
  swc1 $f23, 56($a0)
  and  $a0,  $a0, STORE_MASK
  swc1 $f24, 60($a0)

  and  $a0,  $a0, STORE_MASK
  swc1 $f25, 64($a0)
  and  $a0,  $a0, STORE_MASK
  swc1 $f26, 68($a0)

  and  $a0,  $a0, STORE_MASK
  swc1 $f27, 72($a0)
  and  $a0,  $a0, STORE_MASK
  swc1 $f28, 76($a0)

  and  $a0,  $a0, STORE_MASK
  swc1 $f29, 80($a0)
  and  $a0,  $a0, STORE_MASK
  swc1 $f30, 84($a0)

  and  $a0,  $a0, STORE_MASK
  swc1 $f31, 88($a0)
  /* NACL_CALLEE_SAVE_LIST END */
  move $v0, $zero
  nop

  and  $ra, $ra, JUMP_MASK
  j    $ra
  nop
  nop

  .end setjmp

  .text
  .p2align 4
  .align NACL_BLOCK_SHIFT
  .globl longjmp
  .ent longjmp
  .set noreorder

/* void longjmp(jmp_buf env, int val) */
longjmp:

  /* NACL_CALLEE_LOAD_LIST BEGIN */
  and  $a0, $a0, STORE_MASK
  lw   $s0, 0($a0)
  and  $a0, $a0, STORE_MASK
  lw   $s1, 4($a0)

  and  $a0, $a0, STORE_MASK
  lw   $s2, 8($a0)
  and  $a0, $a0, STORE_MASK
  lw   $s3, 12($a0)

  and  $a0, $a0, STORE_MASK
  lw   $s4, 16($a0)
  and  $a0, $a0, STORE_MASK
  lw   $s5, 20($a0)

  and  $a0, $a0, STORE_MASK
  lw   $s6, 24($a0)
  and  $a0, $a0, STORE_MASK
  lw   $s7, 28($a0)

  and  $a0, $a0, STORE_MASK
  lw   $sp, 32($a0)
  and  $sp, $sp, STORE_MASK
  nop

  and  $a0, $a0, STORE_MASK
  lw   $fp, 36($a0)
  and  $a0, $a0, STORE_MASK
  lw   $ra, 40($a0)

  and  $a0,  $a0, STORE_MASK
  lwc1 $f20, 44($a0)
  and  $a0,  $a0, STORE_MASK
  lwc1 $f21, 48($a0)

  and  $a0,  $a0, STORE_MASK
  lwc1 $f22, 52($a0)
  and  $a0,  $a0, STORE_MASK
  lwc1 $f23, 56($a0)

  and  $a0,  $a0, STORE_MASK
  lwc1 $f24, 60($a0)
  and  $a0,  $a0, STORE_MASK
  lwc1 $f25, 64($a0)

  and  $a0,  $a0, STORE_MASK
  lwc1 $f26, 68($a0)
  and  $a0,  $a0, STORE_MASK
  lwc1 $f27, 72($a0)

  and  $a0,  $a0, STORE_MASK
  lwc1 $f28, 76($a0)
  and  $a0,  $a0, STORE_MASK
  lwc1 $f29, 80($a0)

  and  $a0,  $a0, STORE_MASK
  lwc1 $f30, 84($a0)
  and  $a0,  $a0, STORE_MASK
  lwc1 $f31, 88($a0)
  /* NACL_CALLEE_LOAD_LIST END */

  bne  $a1, $zero, 1f
  nop
  ori  $a1, $zero, 1
1:
  move $v0, $a1

  and  $ra, $ra, JUMP_MASK
  j    $ra
  nop
  nop

  .end longjmp

