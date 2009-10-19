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

/*
 * Trampoline to transfer control from native client module to
 * sel_ldr's NaClSyscallSeg residing in the service runtime portion of address
 * space. Trampolines are patched into nacl module's address space in the
 * trampoline region. They are patched by NaClLoadTrampoline() code (sel_ldr.c).
 * Each trampoline code segment corresponds to a system call, so the trampoline
 * region is full of identical trampoline code segments. Service runtime
 * distinguish which system call is requested using the address of an executed
 * trampoline (it is saved on stack in NaClSyscallSeg()).
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"


  .text

.globl  IDENTIFIER(NaCl_trampoline_seg_code)
  HIDDEN(NaCl_trampoline_seg_code)
IDENTIFIER(NaCl_trampoline_seg_code):
/*
 * ARM passes parameters to a callee in registers r0-r3. If there are more
 * than 4 parameters, the first four args are passed in registers the rest are
 * placed on the stack. This code pushes all parameters from registers into the
 * stack; thus, we keep all parameters on the stack as follows:
 * top - arg0, arg1, arg2, arg3 .... argN
 *
 * On top of that we push the return address, so we will know where to return
 * after the system call.
 *
 * After this code the stack layout must look like:
 *
 * sp+0: retaddr to user module
 * sp+4: arg0 (system call argument)
 * sp+8: arg1
 * ..........
 * sp+X: argN
 *
 * When service runtime serves a system call, it first creates a structure which
 * utilizes these arguments. The structure is created by Decoder functions in
 * nacl_syscall_handlers.c. (nacl_syscall_handlers.c is an automatically
 * generated file and placed in
 * scons-out//gen/native_client/src/trusted/service_runtime).
 *
 */

  push {r0, r1, r2, r3}
  push {lr}

  ldr r0, [pc, #4]
  blx r0

  /* NORETURN */
  NACL_HALT

  .word   NaClSyscallSeg
.globl  IDENTIFIER(NaCl_trampoline_seg_end)
  HIDDEN(NaCl_trampoline_seg_end)
IDENTIFIER(NaCl_trampoline_seg_end):
