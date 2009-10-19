/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

  .text
.globl  IDENTIFIER(NaClSwitch)
  HIDDEN(NaClSwitch)
IDENTIFIER(NaClSwitch):

  /*
   * This code is executed when switching from the service runtime to a nacl
   * module. This happens when a main nacl thread is created and starting to
   * execute the nacle code, or when nacl module is returning from a system
   * call. This piece of code lives in a service runtime part of address space.
   * the one and only argument is in register r0
   *
   * r0 -- address of thread context (struct NaClThreadContext)
   */

  /* clear registers r2,r3, lr, flag and status fields in CPSR (status
   * register). This might be necessary to avoid information leaks, the rest
   * register are overwritten by the following code and the code in springboard.
   */
  mov r2, #0
  mov r3, #0
  mov lr, #0
  msr cpsr_fs, #0

  ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, fp, sp}
  add r0, #4 /* skip prog_ctr in struct NaClThreadContext */
  ldmia r0, {r0, r1, pc}
