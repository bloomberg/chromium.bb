/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/arch/x86/nacl_ldt_x86.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_tls.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


static void NaClThreadStartupCheck() {
  CHECK(sizeof(struct NaClThreadContext) == 0x48);
}


int NaClTlsInit() {
  NaClThreadStartupCheck();
  return NaClLdtInit();
}


void NaClTlsFini() {
  NaClLdtFini();
}


uint32_t NaClTlsAllocate(struct NaClAppThread *natp,
                         void                 *base_addr) {
  UNREFERENCED_PARAMETER(natp);
  return (uint32_t) NaClLdtAllocateByteSelector(NACL_LDT_DESCRIPTOR_DATA,
                                                0,
                                                base_addr,
                                                4);
}


void NaClTlsFree(struct NaClAppThread *natp) {
  NaClLdtDeleteSelector(natp->user.gs);
}


uint32_t NaClTlsChange(struct NaClAppThread *natp,
                       void                 *base_addr) {
  return (uint32_t)NaClLdtChangeByteSelector(natp->user.gs >> 3,
                                             NACL_LDT_DESCRIPTOR_DATA,
                                             0,
                                             base_addr,
                                             4);
}


uint32_t NaClGetThreadIdx(struct NaClAppThread *natp) {
  return natp->user.gs >> 3;
}

/*
 * This is a NOOP, since TLS (or TSD) is not used to keep the thread
 * index on the x86-32.  We use segmentation (%gs) to provide access
 * to the per-thread data, and the segment selector itself tells us
 * the thread's identity.
 */
void NaClTlsSetIdx(uint32_t tls_idx) {
  UNREFERENCED_PARAMETER(tls_idx);
}
