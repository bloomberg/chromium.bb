/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <mach/mach.h>
#include <mach/thread_status.h>

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
  DCHECK(natp->suspend_state == (Atomic32) old_state);
  natp->suspend_state = new_state;
  NaClXMutexUnlock(&natp->suspend_mu);
}

void NaClUntrustedThreadSuspend(struct NaClAppThread *natp,
                                int save_registers) {
  /*
   * We claim suspend_mu here to block trusted/untrusted context
   * switches by blocking NaClAppThreadSetSuspendState().  This blocks
   * any untrusted->trusted context switch that might happen before
   * SuspendThread() takes effect.  It blocks any trusted->untrusted
   * context switch that might happen if the syscall running in the
   * target thread returns.
   */
  NaClXMutexLock(&natp->suspend_mu);
  if (natp->suspend_state == NACL_APP_THREAD_UNTRUSTED) {
    kern_return_t result;
    mach_msg_type_number_t size;
    x86_thread_state_t regs;
    mach_port_t thread_port = pthread_mach_thread_np(natp->thread.tid);

    result = thread_suspend(thread_port);
    if (result != KERN_SUCCESS) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadSuspend: "
              "thread_suspend() call failed\n");
    }

    if (save_registers) {
      if (natp->suspended_registers == NULL) {
        natp->suspended_registers = malloc(sizeof(*natp->suspended_registers));
        if (natp->suspended_registers == NULL) {
          NaClLog(LOG_FATAL, "NaClUntrustedThreadSuspend: malloc() failed\n");
        }
      }

      size = sizeof(regs) / sizeof(natural_t);
      result = thread_get_state(thread_port, x86_THREAD_STATE,
                                (void *) &regs, &size);
      if (result != KERN_SUCCESS) {
        NaClLog(LOG_FATAL, "NaClUntrustedThreadSuspend: "
                "thread_get_state() call failed\n");
      }
      NaClSignalContextFromMacThreadState(natp->suspended_registers, &regs);
    }
  }
  /*
   * We leave suspend_mu held so that NaClAppThreadSetSuspendState()
   * will block.
   */
}

void NaClUntrustedThreadResume(struct NaClAppThread *natp) {
  if (natp->suspend_state == NACL_APP_THREAD_UNTRUSTED) {
    mach_port_t thread_port = pthread_mach_thread_np(natp->thread.tid);
    if (thread_resume(thread_port) != 0) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadSuspend: "
              "thread_resume() call failed\n");
    }
  }
  NaClXMutexUnlock(&natp->suspend_mu);
}

int NaClAppThreadUnblockIfFaulted(struct NaClAppThread *natp, int *signal) {
  UNREFERENCED_PARAMETER(natp);
  UNREFERENCED_PARAMETER(signal);

  NaClLog(LOG_FATAL, "NaClAppThreadUnblockIfFaulted: Not implemented on Mac\n");
  return 0;
}
