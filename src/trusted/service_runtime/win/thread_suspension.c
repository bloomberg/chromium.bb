/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <windows.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"


void NaClAppThreadSetSuspendState(struct NaClAppThread *natp,
                                  enum NaClSuspendState old_state,
                                  enum NaClSuspendState new_state) {
  /*
   * Claiming suspend_mu here blocks a trusted/untrusted context
   * switch while the thread is suspended or a suspension is in
   * progress.
   */
  NaClXMutexLock(&natp->suspend_mu);
  DCHECK(natp->suspend_state == old_state);
  natp->suspend_state = new_state;
  NaClXMutexUnlock(&natp->suspend_mu);
}

void NaClUntrustedThreadSuspend(struct NaClAppThread *natp,
                                int save_registers) {
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
  NaClXMutexLock(&natp->suspend_mu);
  if (natp->suspend_state == NACL_APP_THREAD_UNTRUSTED) {
    CONTEXT context;
    if (SuspendThread(natp->thread.tid) == (DWORD) -1) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadsSuspend: "
              "SuspendThread() call failed\n");
    }
    /*
     * SuspendThread() can return before the thread has been
     * suspended, because internally it only sends a message asking
     * for the thread to be suspended.
     * See http://code.google.com/p/nativeclient/issues/detail?id=2557
     *
     * As a workaround for that, we call GetThreadContext() even when
     * save_registers=0.  GetThreadContext() should only be able to
     * return a snapshot of the register state once the thread has
     * actually suspended.
     *
     * If save_registers=0, the set of registers we request via
     * ContextFlags is unimportant as long as it is non-empty.
     */
    context.ContextFlags = CONTEXT_CONTROL;
    if (save_registers) {
      context.ContextFlags |= CONTEXT_INTEGER;
    }
    if (!GetThreadContext(natp->thread.tid, &context)) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadsSuspend: "
              "GetThreadContext() failed\n");
    }
    if (save_registers) {
      if (natp->suspended_registers == NULL) {
        natp->suspended_registers = malloc(sizeof(*natp->suspended_registers));
        if (natp->suspended_registers == NULL) {
          NaClLog(LOG_FATAL, "NaClUntrustedThreadSuspend: malloc() failed\n");
        }
      }
      NaClSignalContextFromHandler(natp->suspended_registers, &context);
    }
  }
  /*
   * We leave suspend_mu held so that NaClAppThreadSetSuspendState()
   * will block.
   */
}

void NaClUntrustedThreadResume(struct NaClAppThread *natp) {
  if (natp->suspend_state == NACL_APP_THREAD_UNTRUSTED) {
    if (ResumeThread(natp->thread.tid) == (DWORD) -1) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadsResume: "
              "ResumeThread() call failed\n");
    }
  }
  NaClXMutexUnlock(&natp->suspend_mu);
}
