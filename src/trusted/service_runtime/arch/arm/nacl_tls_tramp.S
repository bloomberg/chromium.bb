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
  HIDDEN(NaClReadTP_start)
IDENTIFIER(NaClReadTP_start):

  /* r9 keeps the TLS_IDX which is concatenation of a tdb pointer and index in
   * nacl_thread/nacl_user/nacl_sys arrays. The higher 20 bits are for tdb
   * address and the lower 12 keep the index.
   * The first word of the tdb points to the tls are for the thread.
   *
   * This code is copied into nacl module address space by
   * sel_ldr_arm::NaClLoadTlsHook()
   * The target location is just below  NACL_TRAMPOLINE_END
   *
   * NOTE: A copy of this code also exists in the arm startup under
   *       the arm eabi name  __aeabi_read_tp()
   */
  mov r0, r9, lsr #NACL_PAGESHIFT
  lsl r0, #NACL_PAGESHIFT
#if !defined(DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX)
  bic lr, lr, #0xF000000F
#endif
  bx lr

.globl  IDENTIFIER(NaClReadTP_end)
  HIDDEN(NaClReadTP_end)
IDENTIFIER(NaClReadTP_end):
