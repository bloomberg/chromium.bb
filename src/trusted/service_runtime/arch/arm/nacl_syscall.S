/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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

  /*
   * Load the __thread variable's offset into r3.  We're quite sure that
   * our offsets will be small and so this could fit into an immediate
   * rather than using this load here.  But the only kind of TLS reloc
   * available is the full-word one, not one that can go inside an
   * immediate (and there's no assembly syntax to permit it either).
   */
#ifdef __PIC__
  ldr  r3, .L.gNaClThreadIdxPIC
.LPIC0:
  add  r3, pc
  ldr  r3, [r3]
#else
  ldr  r3, .L.gNaClThreadIdx
#endif

  push {lr}

  /*
   * Fetch the thread-local variable holding the thread index.
   * TODO(mcgrathr): On newer ARM chips, perhaps detectable by some #if
   * (ones where -mtp=auto chooses -mtp=cp15), we could directly use:
   *    mrc p15, 0, r0, c13, c0, 3
   * instead of the call here.
   *
   * __aeabi_read_tp is not allowed to clobber any registers but r0.
   * We're also sure that this call doesn't use the stack, which would
   * be unsafe since it's the untrusted stack and could be clobbered
   * by a racing thread to hijack us.  We verify that in the platform
   * qualification tests, for paranoia's sake.
   */
  bl  __aeabi_read_tp
  ldr  r0, [r0, r3]

#ifdef __PIC__
  ldr  r3, .L.GOT
  ldr  r1, .L.nacl_user
  ldr  r2, .L.nacl_sys
.LPIC1:
  add  r3, pc
  ldr  r1, [r3, r1]
#else
  ldr  r1, =nacl_user
#endif
  ldr  r1, [r1, r0, lsl #2]
  /* r1 = nacl_user[r0*4], r1 points to the nacl_user entry of this thread */

  /* store thread state to the nacl_user entry */
  stmia r1, NACL_CALLEE_SAVE_LIST

#ifdef __PIC__
  ldr r1, [r3, r2]
#else
  ldr r1, =nacl_sys
#endif
  ldr r1, [r1, r0, lsl #2]
  /* r1 = nacl_sys[r0*4], r1 points to the nacl_sys entry of this thread */

  /* restore the service runtime state from nacl_sys entry */
  ldmia r1, NACL_CALLEE_SAVE_LIST

  bl  NaClSyscallCSegHook

  /* NOTREACHED */

  /*
   * if the thread returns, which must not happen, it will be halted
   * by the following instruction
   */

  NACL_HALT

  .balign 4
#ifdef __PIC__
.L.gNaClThreadIdxPIC:
  .word  gNaClThreadIdx(gottpoff) + (. - .LPIC0 - 8)
.L.GOT:
  .word  _GLOBAL_OFFSET_TABLE_ - (.LPIC1 + 8)
.L.nacl_user:
  .word  nacl_user(GOT)
.L.nacl_sys:
  .word  nacl_sys(GOT)
#else
.L.gNaClThreadIdx:
  .word  gNaClThreadIdx(tpoff)
#endif
