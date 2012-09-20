/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/thread_capture/thread_capture_test_injection.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/tests/thread_capture/arch/x86_32/nacl_test_capture_32.h"

void NaClInjectThreadCaptureTrampoline(struct NaClApp *nap) {
  struct NaClPatchInfo patch_info;
  struct NaClPatch patch16[1];

  NaClLog(LOG_WARNING, "Installing ThreadCapture test trampoline\n");

  patch16[0].target = ((uintptr_t) NaClTestCaptureGs) - 2;
  patch16[0].value = NaClGetGs();
  NaClPatchInfoCtor(&patch_info);
  patch_info.abs16 = patch16;
  patch_info.num_abs16 = NACL_ARRAY_SIZE(patch16);
  patch_info.dst = nap->mem_start + NACL_SYSCALL_START_ADDR +
      NACL_SYSCALL_BLOCK_SIZE * NACL_sys_test_syscall_1;
  patch_info.src = (uintptr_t) NaClTestCapture;
  patch_info.nbytes = (uintptr_t) NaClTestCaptureEnd -
      (uintptr_t) NaClTestCapture;
  DCHECK(patch_info.nbytes <= (size_t) nap->bundle_size);
  NaClLog(4,
          ("NaClInjectThreadCaptureTrampoline: installing test_capture"
           " from %"NACL_PRIxPTR" to %"NACL_PRIxPTR"\n"),
          (uintptr_t) NaClTestCapture, patch_info.dst);
  NaClApplyPatchToMemory(&patch_info);
}
