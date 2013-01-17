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
#if NACL_DANGEROUS_USE_PTHREAD_GETSPECIFIC_ON_ANDROID
  push {lr}
  bl  NaClTlsGetIdx
#else
  /*
   * Load the __thread variable's offset into r3.  We're quite sure that
   * our offsets will be small and so this could fit into an immediate
   * rather than using this load here.  But the only kind of TLS reloc
   * available is the full-word one, not one that can go inside an
   * immediate (and there's no assembly syntax to permit it either).
   */
  ldr  r3, .L.gNaClThreadIdxPIC
.LPIC0:
  add  r3, pc
  ldr  r3, [r3]

  push {lr}

  /*
   * Fetch the thread-local variable holding the thread index.
   * This funny instruction is what the compiler generates for -mtp=cp15,
   * which is the default for ARMv7 code generation.
   */
  mrc  p15, 0, r0, c13, c0, 3
  ldr  r0, [r0, r3]
#endif /* !NACL_DANGEROUS_USE_PTHREAD_GETSPECIFIC_ON_ANDROID */

  ldr  r3, .L.GOT
  ldr  r1, .L.nacl_user
.LPIC1:
  add  r3, pc
  ldr  r1, [r3, r1]
  ldr  r1, [r1, r0, lsl #2]
  /* r1 = nacl_user[r0*4], r1 points to the nacl_user entry of this thread */

  /* Adjust the stack pointer to account for values pushed by trusted code. */
  add sp, sp, #0x18
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSyscallThreadCaptureFault):
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

  .balign 4
#if !NACL_DANGEROUS_USE_PTHREAD_GETSPECIFIC_ON_ANDROID
.L.gNaClThreadIdxPIC:
  .word  gNaClThreadIdx(gottpoff) + (. - .LPIC0 - 8)
#endif /* !NACL_DANGEROUS_USE_PTHREAD_GETSPECIFIC_ON_ANDROID */
.L.GOT:
  .word  _GLOBAL_OFFSET_TABLE_ - (.LPIC1 + 8)
.L.nacl_user:
  .word  nacl_user(GOT)
