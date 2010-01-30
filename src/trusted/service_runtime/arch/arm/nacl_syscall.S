/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

/*
 * This trusted code is linked into the service runtime. It is executed when a
 * nacl module performs a system call via service runtime interface. The nacl
 * module jumps to trampoline corresponding to the syscall it performs and then
 * here. This code switches the execution contexts (registers and stack) from
 * untrusted to trusted.
 *
 * on stack:
 *
 * top
 * =====
 * r0 - alignment for NaClSyscallCSegHook
 * lr - return address
 * arg 1
 * arg 2
 * .....
 * arg N
 */

.globl  IDENTIFIER(NaClSyscallSeg)
  HIDDEN(NaClSyscallSeg)
IDENTIFIER(NaClSyscallSeg):

  push {lr}

  /*
   * TLS_IDX represents address of a TLS region and index in
   * nacl_thread/nacl_user/nacl_sys arrays. The TLS region is paged aligned;
   * therefore, we use the lowest 12 bits to store the thread index
   */

  /* r9 keep lower 12 bits =  TLS_IDX */
  mov r0, r9, lsl #(32 - NACL_PAGESHIFT)
  lsr r0, #(32 - NACL_PAGESHIFT)

  ldr  r1, =nacl_user
  ldr  r1, [r1, r0, lsl #2]
  /* r1 = nacl_user[r0*4], r1 points to the nacl_user entry of this thread */

  /* store thread state to the nacl_user entry */
  stmia r1, {r4, r5, r6, r7, r8, r9, r10, r11, sp}

  ldr r1, =nacl_sys
  ldr r1, [r1, r0, lsl #2]
  /* r1 = nacl_sys[r0*4], r1 points to the nacl_sys entry of this thread */

  /* restore the service runtime state from nacl_sys entry */
  ldmia r1, {r4, r5, r6, r7, r8, r9, r10, r11, sp}

  bl  NaClSyscallCSegHook

  /* NOTREACHED */

  /*
   * if the thread returns, which must not happen, it will be halted
   * by the following instruction
   */

  NACL_HALT
