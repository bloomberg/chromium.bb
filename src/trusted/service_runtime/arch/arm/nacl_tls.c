/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_ldr_arm.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
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
