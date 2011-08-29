# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#include "../testdata-thumb/masks.h"
.syntax unified
.code 16
.thumb_func
.global _start
_start:
hello_world:
  /* Call write. */
  movs r0, #1  /* stdout */
  adr r1, hwstring - 2
  movs r2, #13 /* strlen */
  b do_write
  nop
  nop
base:
  bkpt
hwstring:
  .asciz "hello, world\n"
do_write:
  ldr r3, write_tramp_address + 2
  CMASK(r3)
  blx r3
  nop
bundle_boundary:
  b do_exit
  nop
  nop
  nop
  nop
  nop
  nop
base2:
  bkpt
write_tramp_address:
  .word 0x8000 + (13 * 32) + 0xf
exit_tramp_address:
  .word 0x8000 + (30 * 32) + 0xf
  nop
  nop
  nop
do_exit:
  /* Call exit. */
  movs r0, #0  /* success */
  ldr r1, exit_tramp_address  - 2
  CMASK(r1)
  blx r1
