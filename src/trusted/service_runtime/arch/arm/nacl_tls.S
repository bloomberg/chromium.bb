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
 * This code is patched into trampoline region, in the pre-last slot (the last
 * slot is occupied by the springboard). This code is used by the nacl module
 * for obtaining a pointer to its TLS region. A function __aeabi_read_tp in nacl
 * libc must be tweaked to jump to the trampoline slot with this code.
 */

.globl  IDENTIFIER(NaClReadTP_start)
.globl  IDENTIFIER(NaClReadTP_end)

/*
 * TLS_IDX represents address of a TLS region and index in
 * nacl_thread/nacl_user/nacl_sys arrays. The TLS region is paged aligned;
 * therefore, we use the lowest 12 bits to store the thread index
 */

IDENTIFIER(NaClReadTP_start):

#if defined(USE_R9_AS_TLS_REG)
  /* r9 keeps the TLS_IDX */
  mov r0, r9, lsr #NACL_PAGESHIFT
  lsl r0, #NACL_PAGESHIFT
  bx lr
#else
  /*
   * TLS_IDX is stored in service runtime thread local storage, in nacl_tls_idx
   * variable
   */
  push {lr}
  bl NaClGetTls /* returns Tls pointer in r0 */
  pop {pc}
#endif

IDENTIFIER(NaClReadTP_end):

