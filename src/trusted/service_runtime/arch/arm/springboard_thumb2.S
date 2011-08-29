/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"

.text
.thumb
.syntax unified

/*
 * NOTE: it is not clear whether we need a spring board for ARM at all
 * TODO(robertm): maybe delete the code
 *
 * Assembly code template.
 * This is linked into the service runtime but is unused as code -- it is used
 * as data to be patched into a NaCl app's address space as a "hidden" part of
 * its trampoline region.
 *
 * This code takes the last slot in trampoline region, and it is
 * loaded by NaClLoadSpringboard() (sel_ldr.c).
 *
 * NaCl_springboard is used for syscall return and any time we want
 * to do an upcall into NaCl application.
 *
 * r0 -- syscall return value
 * r1 -- new pc (already sandboxed)
 * == user stack ==
 * top
 * =====
 * arg 1
 * arg 2
 * .....
 * arg N
 */

 /*
  * We will load the springboard to be aligned 14mod16, as a thumb2 branch
  * target must be.  To ensure that the instructions in the springboard are
  * properly aligned, we misalign by 2 here.
  */
.p2align 4
.skip 2
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaCl_springboard):
  /*
   * This instruction prevents indirect jumps from untrusted code into the
   * springboard.  The service runtime jumps past it.
   */
  bkpt

  add sp, #16 /* popping four arguments placed by trampoline code */
  bx r1

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaCl_springboard_end):
