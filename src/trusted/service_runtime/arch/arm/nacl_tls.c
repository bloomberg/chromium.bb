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
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_ldr_arm.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_tls.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

/*
 * TlsThreadIdx represents concatenation of a thread's TLS address and a
 * thread's index (in nacl_user,nacl_sys,nacl_thread arrays). The top 20 bits
 * are dedicated to TLS address (which is page-aligned), and the lower 12 bits
 * represents thread index. Because of such encoding we have a limit to a number
 * of threads a nacl module can create. It is (1<<12).
 */

static uint32_t CombinedDescriptorExtractIdx(uint32_t combined) {
  return  combined &  ((1 << NACL_PAGESHIFT) - 1);
}


static uint32_t CombinedDescriptorExtractTdb(uint32_t combined) {
  return  combined &  ~((1 << NACL_PAGESHIFT) - 1);
}

static uint32_t MakeCombinedDescriptor(uint32_t tdb,
                                       uint32_t idx) {
  /* NOTE: we need to work around and issue here where, for the main thread,
     we temporarily have to accomodate an invalid tdb */
  /* TODO(robertm): clean this up */
  if (CombinedDescriptorExtractTdb(tdb) != tdb) {
    NaClLog(LOG_ERROR, "bad tdb alignment tdb %x idx %d\n", tdb, idx);
    NaClLog(LOG_ERROR, "this is expected at startup\n");
    tdb = CombinedDescriptorExtractTdb(~0);
  }

  if (CombinedDescriptorExtractIdx(idx) != idx) {
    NaClLog(LOG_FATAL, "bad thread idx tdb %x idx %d\n", tdb, idx);
  }
  return tdb | idx;
}


uint32_t NaClGetThreadCombinedDescriptor(struct NaClThreadContext *user) {
  return user->r9;
}


void NaClSetThreadCombinedDescriptor(struct NaClThreadContext *user,
                                     uint32_t descriptor) {
  user->r9 = descriptor;
}


uint32_t NaClGetThreadIdx(struct NaClAppThread *natp) {
  uint32_t combined = NaClGetThreadCombinedDescriptor(&natp->user);
  return CombinedDescriptorExtractIdx(combined);
}

#if 0
/* NOTE: currently not used */
static uint32_t NaClGetThreadTdb(struct NaClAppThread *natp) {
  uint32_t combined = NaClGetThreadCombinedDescriptor(&natp->user);
  return CombinedDescriptorExtractTdb(combined);
}
#endif

static void NaClThreadStartupCheck() {
  CHECK(sizeof(struct NaClThreadContext) == 0x34);
}


int NaClTlsInit() {
  int i;
  NaClLog(2, "NaClTlsInit\n");
  NaClThreadStartupCheck();

  for (i = 0; i < NACL_THREAD_MAX; i++)
    nacl_thread[i] = NULL;

  return 1;
}


void NaClTlsFini() {
  NaClLog(2, "NaClTlsFini\n");
}

/* NOTE: the actual management of this pool is done elsewhere */
static int NaClThreadIdxAllocate() {
  int i;

  for (i = 0; i < NACL_THREAD_MAX; i++)
    if (nacl_thread[i] == NULL) return i;

  NaClLog(LOG_ERROR, "NaClAllocateThreadIdx: no more slots for a thread\n");
  return -1;
}


uint32_t NaClTlsAllocate(struct NaClAppThread *natp,
                         void *tdb,
                         uint32_t size) {
  uint32_t idx = NaClThreadIdxAllocate();
  UNREFERENCED_PARAMETER(natp);

  NaClLog(2,
          "NaClTlsAllocate: tdb %x size %d idx %d\n",
          (uint32_t) tdb, size, idx);
  if (CombinedDescriptorExtractIdx(idx) != idx) {
    NaClLog(LOG_FATAL,
            "cannot allocate new thread idx tdb %x\n",
            (uint32_t)tdb);
    return ~0;
  }

  return MakeCombinedDescriptor((uint32_t) tdb, idx);
}


void NaClTlsFree(struct NaClAppThread *natp) {
  NaClLog(2,
          "NaClTlsFree: old %x\n",
          NaClGetThreadCombinedDescriptor(&natp->user));
  NaClSetThreadCombinedDescriptor(&natp->user, 0);
}


uint32_t NaClTlsChange(struct NaClAppThread *natp,
                       void *tdb,
                       uint32_t size) {
  uint32_t idx = NaClGetThreadIdx(natp);
  uint32_t combined = MakeCombinedDescriptor((uint32_t) tdb, idx);
  NaClLog(2,
          "NaClTlsChange: tdb %x size %d idx %d\n",
          (uint32_t) tdb, size, idx);

  NaClSetThreadCombinedDescriptor(&natp->user, combined);
  return combined;
}
