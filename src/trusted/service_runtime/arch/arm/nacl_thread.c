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

#include <stdio.h>
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"


int NaClThreadInit() {
  int i;

  for (i = 1; i < LDT_ENTRIES; i++)
    nacl_user[i] = 0;

  return 1;
}


void NaClThreadFini() {
}


uint16_t NaClAllocateThreadIdx(int type,
                               int read_exec_only,
                               void *base_addr,
                               uint32_t size_in_bytes) {
  int i;

  for (i = 1; i < LDT_ENTRIES; i++)
    if (!nacl_user[i]) return i;

  /* no more free entries */
  return 0;
}


void NaClFreeThreadIdx(struct NaClAppThread *natp) {
  uint16_t idx = natp->user.r9;

  if (idx < LDT_ENTRIES)
    nacl_user[idx] = 0;
  else ASSERT(0);
}


uint16_t NaClChangeThreadIdx(struct NaClAppThread *natp,
                             int type,
                             int read_exec_only,
                             void* base_addr,
                             uint32_t size_in_bytes) {
  /* BUG(petr): not implemented */
  ASSERT(0);
  return 0;
}


int16_t NaClGetThreadIdx(struct NaClAppThread *natp) {
  return natp->user.r9;
}

