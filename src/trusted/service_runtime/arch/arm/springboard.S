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
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"

  /*
   * Assembly code template.  This is linked into the service runtime
   * but is unused as code -- it is used as data to be patched into
   * a NaCl app's address space as a "hidden" part of its trampoline
   * region. This code takes the last slot in trampoline region, and it is
   * loaded by NaClLoadSpringboard() (sel_ldr.c).
   */

  /*
   * NaCl_springboard is used for syscall return and any time we want
   * to do an upcall into NaCl application.
   */

.globl  IDENTIFIER(NaCl_springboard)
  HIDDEN(NaCl_springboard)
IDENTIFIER(NaCl_springboard):
  /*
   * r0 -- syscall return value
   * r1 -- new pc
   * == user srack ==
   * top
   * =====
   * arg 1
   * arg 2
   * .....
   * arg N
   */

  /*
   * This instruction guards springboard from unauthorized execution.
   * Service runtime jumps to the instruction following the NACL_HALT one. If
   * any other code tries to reach springboard without performing a jump it will
   * be halted by the NACL_HALT
   */
  NACL_HALT

  add sp, #16 /* popping four arguments placed by trampoline code */
#if !defined(DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX)
  bic r1, r1, #NACL_CONTROL_FLOW_MASK
#endif
  bx r1
.globl  IDENTIFIER(NaCl_springboard_end)
  HIDDEN(NaCl_springboard_end)
IDENTIFIER(NaCl_springboard_end):
