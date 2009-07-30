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

#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

#define TLS_IDX_GET_ADDR(x)  (x & ~((1 << NACL_PAGESHIFT) - 1))
#define TLS_IDX_GET_IDX(x)   (x &  ((1 << NACL_PAGESHIFT) - 1))

#if !defined(USE_R9_AS_TLS_REG)
__thread uint32_t nacl_tls_idx;


uint32_t NaClGetThreadIndex() {
  return TLS_IDX_GET_IDX(nacl_tls_idx);
}


uint32_t NaClGetTls() {
  return TLS_IDX_GET_ADDR(nacl_tls_idx);
}
#endif


int NaClTlsInit() {
  int i;

  for (i = 0; i < NACL_THREAD_MAX; i++)
    nacl_thread[i] = NULL;

  return 1;
}


void NaClTlsFini() {
}


uint32_t NaClGetTlsIdx(struct NaClThreadContext *user) {
#if defined(USE_R9_AS_TLS_REG)
  return user->r9;
#else
  UNREFERENCED_PARAMETER(user);
  return nacl_tls_idx;
#endif
}

void NaClSetTlsIdx(struct NaClThreadContext *user, uint32_t tls_idx) {
#if defined(USE_R9_AS_TLS_REG)
  user->r9 = tls_idx;
#else
  UNREFERENCED_PARAMETER(user);
  nacl_tls_idx = tls_idx;
#endif
}


static int NaClAllocateThreadIndex() {
  int i;

  for (i = 0; i < NACL_THREAD_MAX; i++)
    if (nacl_thread[i] == NULL) return i;

  NaClLog(LOG_ERROR, "NaClAllocateThreadIndex: "
          "There is no more slots for a thread\n");

  return -1;
}


static void *NaClAllocateTls_intern(struct NaClAppThread *natp,
                                    void *base_addr,
                                    uint32_t size) {
  int rv;

  base_addr = (void *)NaClRoundPage((size_t)base_addr);
  size = NaClRoundPage(size);

  NaClLog(2, "NaClAllocateTls: "
          "new TLS is at 0x%08"PRIx32", size 0x%"PRIx32"\n",
          (uint32_t)base_addr, size);

  rv = NaCl_page_alloc_at_addr(base_addr, size);
  if (rv) {
    NaClLog(LOG_ERROR, "NaClAllocateTls: "
            "NaCl_page_alloc_at_addr 0x%08"PRIxPTR" failed\n",
            (uint32_t)base_addr);
    return NULL;
  }

  rv = NaCl_mprotect(base_addr, size, PROT_READ | PROT_WRITE);
  if (rv) {
    NaClLog(LOG_ERROR, "NaClAllocateTls: "
            "NaCl_mprotect(0x%08"PRIxPTR", 0x%08"PRIx32", 0x%x) failed,"
            "error %d (data)\n",
            (uint32_t)base_addr, size, PROT_READ | PROT_WRITE, rv);
    return NULL;
  }

  natp->user_tls_size = size;
  return base_addr;
}


uint32_t NaClAllocateTls(struct NaClAppThread *natp,
                         void *base_addr,
                         uint32_t size) {
  int idx;

  base_addr = NaClAllocateTls_intern(natp, base_addr, size);
  if (base_addr == NULL) return 0;
  idx = NaClAllocateThreadIndex();
  if (idx < 0) return 0;

  /*
   * tls region is page aligned, and we use 12 least significant bits to keep
   * the thread index of nacl_thread/nacl_user/nacl_sys arrays
   */
  return (uint32_t)base_addr | (uint32_t)idx;
}


void NaClFreeTls(struct NaClAppThread *natp) {
  void *addr;
  uint32_t idx, tls_idx;

  tls_idx = NaClGetTlsIdx(&natp->user);
  NaClSetTlsIdx(&natp->user, 0);

  addr = (void *)TLS_IDX_GET_ADDR(tls_idx);
  idx = TLS_IDX_GET_IDX(tls_idx);

  NaClLog(2, "NaClFreeTls: "
          "free TLS at 0x%08"PRIx32", size 0x%"PRIx32"\n",
          (uint32_t)addr, natp->user_tls_size);
  NaCl_page_free(addr, natp->user_tls_size);
}


uint32_t NaClChangeTls(struct NaClAppThread *natp,
                       void *base_addr,
                       uint32_t size) {
  uint32_t tls_idx = NaClGetTlsIdx(&natp->user);

  NaClFreeTls(natp);
  base_addr = NaClAllocateTls_intern(natp, base_addr, size);
  tls_idx = (uint32_t)base_addr & TLS_IDX_GET_IDX(tls_idx);
  NaClSetTlsIdx(&natp->user, tls_idx);

  return tls_idx;
}


uint32_t NaClTlsToIndex(struct NaClAppThread *natp) {
  uint32_t tls_idx = NaClGetTlsIdx(&natp->user);
  return TLS_IDX_GET_IDX(tls_idx);
}

