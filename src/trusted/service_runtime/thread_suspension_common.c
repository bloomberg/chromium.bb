/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"
#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"

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

void NaClAppThreadGetSuspendedRegisters(struct NaClAppThread *natp,
                                        struct NaClSignalContext *regs) {
  if ((natp->suspend_state & NACL_APP_THREAD_UNTRUSTED) != 0) {
    NaClAppThreadGetSuspendedRegistersInternal(natp, regs);
  } else {
    NaClThreadContextToSignalContext(&natp->user, regs);
  }
}

void NaClAppThreadSetSuspendedRegisters(struct NaClAppThread *natp,
                                        const struct NaClSignalContext *regs) {
  if ((natp->suspend_state & NACL_APP_THREAD_UNTRUSTED) != 0) {
    NaClAppThreadSetSuspendedRegistersInternal(natp, regs);
  } else {
    /* TODO(eaeltsin): can we alter NaClAppThread.user? */
    NaClLog(LOG_WARNING,
            "NaClAppThreadSetSuspendedRegisters: Registers not modified\n");
  }
}

int NaClFaultedThreadQueueEnable(struct NaClApp *nap) {
  nap->enable_faulted_thread_queue = 1;
  return NaClDebugExceptionHandlerEnsureAttached(nap);
}
