/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
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
  CHECK(sizeof(struct NaClThreadContext) == 64);
}


int NaClTlsInit() {
  NaClThreadStartupCheck();
  return NaClLdtInit();
}


void NaClTlsFini() {
  NaClLdtFini();
}


uint32_t NaClTlsAllocate(struct NaClAppThread *natp) {
  return (uint32_t) NaClLdtAllocateByteSelector(NACL_LDT_DESCRIPTOR_DATA,
                                                /* read_exec_only= */ 1,
                                                &natp->tls_values,
                                                sizeof(natp->tls_values));
}


void NaClTlsFree(struct NaClAppThread *natp) {
  NaClLdtDeleteSelector(natp->user.gs);
}


void NaClTlsChange(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
}


uint32_t NaClGetThreadIdx(struct NaClAppThread *natp) {
  return natp->user.gs >> 3;
}

#if NACL_LINUX

/*
 * This TLS variable mirrors nacl_thread_index in the x86-64 sandbox,
 * except that, on x86-32, we only use it for getting the identity of
 * the interrupted thread in a signal handler in the Linux
 * implementation of thread suspension.
 *
 * We should not enable this code on Windows because TLS variables do
 * not work inside dynamically-loaded DLLs -- such as chrome.dll -- on
 * Windows XP.
 */
THREAD uint32_t nacl_thread_index;

void NaClTlsSetIdx(uint32_t tls_idx) {
  nacl_thread_index = tls_idx;
}

uint32_t NaClTlsGetIdx(void) {
  return nacl_thread_index;
}

#else

/*
 * This is a NOOP, since TLS (or TSD) is not used to keep the thread
 * index on the x86-32.  We use segmentation (%gs) to provide access
 * to the per-thread data, and the segment selector itself tells us
 * the thread's identity.
 */
void NaClTlsSetIdx(uint32_t tls_idx) {
  UNREFERENCED_PARAMETER(tls_idx);
}

#endif
