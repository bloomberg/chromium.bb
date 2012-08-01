/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

.text

/*
 * This trusted code is linked into the service_runtime and
 * executed when switching from the service runtime to a nacl
 * module. This happens when a main nacl thread is created and starting to
 * execute the nacle code, or when nacl module is returning from a system
 * call. This piece of code lives in a service runtime part of address space.
 * the one and only argument is in register r0
 *
 * r0 -- address of thread context (struct NaClThreadContext)
 */

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSwitch):

/*
 * We clear registers r2, r3, r12 (ip), lr, flag and status fields in
 * CPSR (status register) to avoid information leaks.  The remaining
 * registers are overwritten by the code that follows after.
 */

  mov r2, #0
  mov r3, #0
  mov r12, #0
  mov lr, #0
  msr cpsr_fs, #0

  ldmia r0!, NACL_CALLEE_SAVE_LIST
  add r0, #4  /* skip prog_ctr in struct NaClThreadContext */
  /* Read sysret and new_prog_ctr from struct NaClThreadContext */
  ldmia r0, {r0, r1}
  /* Transfer control to untrusted code */
  bx r1
