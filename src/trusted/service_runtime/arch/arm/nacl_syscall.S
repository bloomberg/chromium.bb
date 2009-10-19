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

/*
 * This code is in service runtime part of address space. It is executed when a
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
