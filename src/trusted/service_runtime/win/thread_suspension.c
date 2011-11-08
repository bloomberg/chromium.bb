/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <windows.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


/*
 * NaClUntrustedThreadsSuspend() ensures that any untrusted code is
 * temporarily suspended.  This is used when we are about to open a
 * hole in untrusted address space.  In this case, if another trusted
 * thread (outside of control) allocates memory that is placed into
 * that hole, we do not want untrusted code to being to write to that
 * location.
 *
 * If a thread is currently executing a NaCl syscall, we tell the
 * thread not to return to untrusted code yet.  If a thread is
 * currently executing untrusted code, we suspend it.
 *
 * This returns with the lock threads_mu held, because we need to pin
 * the list of threads.  NaClUntrustedThreadsResume() must be called
 * to undo this.
 */
void NaClUntrustedThreadsSuspend(struct NaClApp *nap) {
  size_t index;

  NaClXMutexLock(&nap->threads_mu);

  for (index = 0; index < nap->threads.num_entries; index++) {
    enum NaClSuspendState old_state;
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp == NULL) {
      continue;
    }

    /*
     * Note that if we are being called from a NaCl syscall (which is
     * likely), natp could be the thread we are running in.  That is
     * fine, because this thread will be in the
     * NACL_APP_THREAD_TRUSTED state, and so we will not call
     * SuspendThread() on it.
     */

    /*
     * We do not want the thread to enter a NaCl syscall and start
     * taking locks when SuspendThread() takes effect, so we ask the
     * thread to suspend even if it currently running untrusted code.
     */
    NaClXMutexLock(&natp->mu);
    old_state = natp->suspend_state;
    natp->suspend_state = old_state | NACL_APP_THREAD_SUSPENDING;
    if (old_state == NACL_APP_THREAD_UNTRUSTED) {
      if (SuspendThread(natp->thread.tid) == (DWORD) -1) {
        NaClLog(LOG_FATAL, "NaClUntrustedThreadsSuspend: "
                "SuspendThread() call failed\n");
      }
    }
    NaClXMutexUnlock(&natp->mu);
  }
}

void NaClUntrustedThreadsResume(struct NaClApp *nap) {
  size_t index;
  for (index = 0; index < nap->threads.num_entries; index++) {
    enum NaClSuspendState old_state;
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp == NULL) {
      continue;
    }

    /*
     * We do not need to lock the mutex natp->mu at this point
     * because, since we set NACL_APP_THREAD_SUSPENDING earlier, we
     * are the only thread that is allowed to change
     * natp->suspend_state.
     */
    old_state = natp->suspend_state;
    DCHECK((old_state & NACL_APP_THREAD_SUSPENDING) != 0);
    if (old_state == (NACL_APP_THREAD_UNTRUSTED | NACL_APP_THREAD_SUSPENDING)) {
      if (ResumeThread(natp->thread.tid) == (DWORD) -1) {
        NaClLog(LOG_FATAL, "NaClUntrustedThreadsResume: "
                "ResumeThread() call failed\n");
      }
    }
    natp->suspend_state = old_state & ~NACL_APP_THREAD_SUSPENDING;
    NaClXCondVarSignal(&natp->cv);
  }

  NaClXMutexUnlock(&nap->threads_mu);
}
