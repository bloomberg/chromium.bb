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
#include "native_client/src/trusted/service_runtime/sel_rt.h"

static int32_t TestSyscall(struct NaClAppThread *natp) {
  NaClCopyInDropLock(natp->nap);

  natp->user.gs = natp->user.trusted_gs;

  return 0;
}

void NaClInjectThreadCaptureSyscall(struct NaClApp *nap) {
  UNREFERENCED_PARAMETER(nap);

  NaClAddSyscall(NACL_sys_test_syscall_1, TestSyscall);
}
