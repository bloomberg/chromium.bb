/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/thread_capture/thread_capture_test_injection.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"

static int32_t TestSyscall(struct NaClAppThread *natp) {
  /* TODO(mcgrathr): Use a safe cast here. */
  NaClCPUFeaturesX86 *features = (NaClCPUFeaturesX86 *) natp->nap->cpu_features;
  if (NaClGetCPUFeatureX86(features, NaClCPUFeatureX86_SSE)) {
    g_nacl_syscall_thread_capture_fault_addr =
        (uintptr_t) &NaClSyscallThreadCaptureFaultSSE;
  } else {
    g_nacl_syscall_thread_capture_fault_addr =
        (uintptr_t) &NaClSyscallThreadCaptureFaultNoSSE;
  }

  NaClCopyDropLock(natp->nap);

  natp->user.gs = natp->user.trusted_gs;

  return 0;
}

void NaClInjectThreadCaptureSyscall(struct NaClApp *nap) {
  UNREFERENCED_PARAMETER(nap);

  NaClAddSyscall(NACL_sys_test_syscall_1, TestSyscall);
}
