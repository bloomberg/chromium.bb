/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

void NaClUntrustedThreadsSuspendAllButOne(struct NaClApp *nap,
                                          struct NaClAppThread *natp_to_skip,
                                          int save_registers) {
  size_t index;

  NaClXMutexLock(&nap->threads_mu);

  /*
   * TODO(mseaborn): A possible refinement here would be to use separate loops
   * for initiating and waiting for the suspension of the threads.  This might
   * be faster, since we would not be waiting for each thread to suspend one by
   * one.  It would take advantage of the asynchronous nature of thread
   * suspension.
   */
  for (index = 0; index < nap->threads.num_entries; index++) {
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp != NULL && natp != natp_to_skip) {
      NaClUntrustedThreadSuspend(natp, save_registers);
    }
  }
}

void NaClUntrustedThreadsResumeAllButOne(struct NaClApp *nap,
                                         struct NaClAppThread *natp_to_skip) {
  size_t index;
  for (index = 0; index < nap->threads.num_entries; index++) {
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp != NULL && natp != natp_to_skip) {
      NaClUntrustedThreadResume(natp);
    }
  }

  NaClXMutexUnlock(&nap->threads_mu);
}

void NaClUntrustedThreadsSuspendAll(struct NaClApp *nap, int save_registers) {
  NaClUntrustedThreadsSuspendAllButOne(nap, NULL, save_registers);
}

void NaClUntrustedThreadsResumeAll(struct NaClApp *nap) {
  NaClUntrustedThreadsResumeAllButOne(nap, NULL);
}
