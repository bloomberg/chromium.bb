/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/arch/arm/sel_ldr_arm.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_rt.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

.text


/*
 * This trusted code is linked into the service runtime. It is executed when a
 * nacl module performs a system call via a service runtime interface. The nacl
 * module jumps to the trampoline corresponding to the syscall and then here.
 * This code switches the execution contexts (registers and stack) from
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

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSyscallSeg):
  /* Set r1 to the start of the NaClThreadContext struct. */
  sub r1, r9, #NACL_THREAD_CONTEXT_OFFSET_TLS_VALUE1

  ldr r0, [r1, #NACL_THREAD_CONTEXT_OFFSET_TLS_IDX]

  /*
   * Check that r9 points into a valid NaClThreadContext by checking
   * that the guard_token field matches nacl_guard_token.
   */
  ldr r2, [r1, #NACL_THREAD_CONTEXT_OFFSET_GUARD_TOKEN]
  ldr r3, .L.offset.nacl_guard_token
.LPIC1:
  ldr r3, [pc, r3]
  cmp r2, r3
  bne NaClSyscallThreadCaptureFault

  push {lr}

  /* Adjust the stack pointer to account for values pushed by trusted code. */
  add sp, sp, #0x18
  /* store thread state to the nacl_user entry */
  stmia r1, NACL_CALLEE_SAVE_LIST
  /* if r1 is NULL, we would have faulted here */

  /*
   * Store the untrusted fpscr state and load the trusted fpscr into r2.
   */
  fmrx ip, fpscr
  str ip, [r1, #NACL_THREAD_CONTEXT_OFFSET_FPSCR]
  ldr r2, [r1, #NACL_THREAD_CONTEXT_OFFSET_SYS_FPSCR]

  /* Restore the trusted stack */
  ldr  sp, [r1, #NACL_THREAD_CONTEXT_OFFSET_TRUSTED_STACK_PTR]

  /* Restore the trusted fpscr. */
  fmxr fpscr, r2

  bl  NaClSyscallCSegHook

  /* NOTREACHED */

  /*
   * if the thread returns, which must not happen, it will be halted
   * by the following instruction
   */

  .word NACL_HALT_WORD

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSyscallThreadCaptureFault):
  .word NACL_HALT_WORD

.L.offset.nacl_guard_token:
  .word nacl_guard_token - (.LPIC1 + 8)
