/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime threads implementation layer.
 */

#include <process.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

int NaClThreadCtor(struct NaClThread  *ntp,
                   void               (WINAPI *start_fn)(void *),
                   void               *state,
                   size_t             stack_size) {
  HANDLE handle;
  DWORD actual_stack_size;

  if (stack_size > MAXDWORD) {
    NaClLog(LOG_ERROR,
      "nacl_thread: _beginthreadex failed, stack request out of range",
      errno);
    return 0;
  } else {
    actual_stack_size = (DWORD)stack_size;
  }

  handle = (HANDLE) _beginthreadex(NULL,      /* default security */
                                   actual_stack_size,
                                   (unsigned (WINAPI *)(void *))  start_fn,
                                   /* the argument for the thread function */
                                   state,
                                   0,         /*start running */
                                   NULL);
  if (0 == handle){  /* we don't need the thread id */
    NaClLog(LOG_ERROR,
            "nacl_thread: _beginthreadex failed, errno %d",
            errno);
    return 0;
  }
  ntp->tid = handle;  /* we need the handle to kill the thread etc. */

  return 1;
}

void NaClThreadDtor(struct NaClThread *ntp) {
  /*
   * the handle is not closed when the thread exits because we are
   * using _beginthreadex and not _beginthread, so we must close it
   * manually
   */
  CloseHandle(ntp->tid);
}

void NaClThreadExit(void) {
  _endthreadex(0);
}

void NaClThreadKill(struct NaClThread *target) {
  TerminateThread(target->tid, 0);
}

int NaClTsdKeyCreate(struct NaClTsdKey  *tsdp) {
  int key;

  key = TlsAlloc();

  if (key == TLS_OUT_OF_INDEXES) {
    NaClLog(LOG_ERROR,
            "NaClTsdKeyCreate: could not create new key, error code %d",
            GetLastError());
    return 0;
  }

  tsdp->key = key;
  return 1;
}

int NaClTsdSetSpecific(struct NaClTsdKey  *tsdp,
                       void const         *ptr) {
  int res;

  res = TlsSetValue(tsdp->key, (void *) ptr);  /* const_cast<void *>(ptr) */

  if (0 == res) {
    NaClLog(LOG_ERROR,
            "NaClTsdSetSpecific: could not set new value, error code %d",
            res);
    return 0;
  }
  return 1;
}

void *NaClTsdGetSpecific(struct NaClTsdKey  *tsdp) {
  return TlsGetValue(tsdp->key);
}

uint32_t NaClThreadId(void) {
  return GetCurrentThreadId();
}
