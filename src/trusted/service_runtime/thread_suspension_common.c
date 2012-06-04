/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

/*
 * NaClUntrustedThreadsSuspend() ensures that any untrusted code is
 * temporarily suspended.
 *
 * If a thread is currently executing a NaCl syscall, we tell the
 * thread not to return to untrusted code yet.  If a thread is
 * currently executing untrusted code, we suspend it.
 *
 * This returns with the lock threads_mu held, because we need to pin
 * the list of threads.  NaClUntrustedThreadsResume() must be called
 * to undo this.
 */
void NaClUntrustedThreadsSuspendAll(struct NaClApp *nap, int save_registers) {
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
    if (natp != NULL) {
      NaClUntrustedThreadSuspend(natp, save_registers);
    }
  }
}

void NaClUntrustedThreadsResumeAll(struct NaClApp *nap) {
  size_t index;
  for (index = 0; index < nap->threads.num_entries; index++) {
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp != NULL) {
      NaClUntrustedThreadResume(natp);
    }
  }

  NaClXMutexUnlock(&nap->threads_mu);
}
