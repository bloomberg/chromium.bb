/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/arch/x86/nacl_ldt_x86.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_tls.h"


static void NaClThreadStartupCheck() {
  CHECK(sizeof(struct NaClThreadContext) == 64);
}


int NaClTlsInit() {
  NaClThreadStartupCheck();
  return NaClLdtInit();
}


void NaClTlsFini() {
  NaClLdtFini();
}


uint32_t NaClTlsAllocate(struct NaClAppThread *natp,
                         void *base_addr,
                         uint32_t size) {
  UNREFERENCED_PARAMETER(natp);
  return (uint32_t)NaClLdtAllocateByteSelector(NACL_LDT_DESCRIPTOR_DATA,
                                               0,
                                               base_addr,
                                               size);
}


void NaClTlsFree(struct NaClAppThread *natp) {
  NaClLdtDeleteSelector(natp->user.gs);
}


uint32_t NaClTlsChange(struct NaClAppThread *natp,
                       void *base_addr,
                       uint32_t size) {
  return (uint32_t)NaClLdtChangeByteSelector(natp->user.gs >> 3,
                                             NACL_LDT_DESCRIPTOR_DATA,
                                             0,
                                             base_addr,
                                             size);
}


uint32_t NaClGetThreadIdx(struct NaClAppThread *natp) {
  return natp->user.gs >> 3;
}
