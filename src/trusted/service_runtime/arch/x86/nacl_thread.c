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

#include "native_client/src/trusted/service_runtime/nacl_ldt.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"


int NaClThreadInit() {
  return NaClLdtInit();
}


void NaClThreadFini() {
  NaClLdtFini();
}


uint16_t NaClAllocateThreadIdx(int type,
                               int read_exec_only,
                               void *base_addr,
                               uint32_t size_in_bytes) {
  return NaClLdtAllocateByteSelector(type, read_exec_only,
                                     base_addr, size_in_bytes);
}


void NaClFreeThreadIdx(struct NaClAppThread *natp) {
  NaClLdtDeleteSelector(natp->user.gs);
}


uint16_t NaClChangeThreadIdx(struct NaClAppThread *natp,
                             int type,
                             int read_exec_only,
                             void* base_addr,
                             uint32_t size_in_bytes) {
  return NaClLdtChangeByteSelector(natp->user.gs >> 3,
                                   type,
                                   read_exec_only,
                                   base_addr,
                                   size_in_bytes);
}


int16_t NaClGetThreadIdx(struct NaClAppThread *natp) {
  return natp->user.gs >> 3;
}

