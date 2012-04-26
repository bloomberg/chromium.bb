/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/win/nacl_fast_mutex.h"

int NaClFastMutexCtor(struct NaClFastMutex *flp) {
  InitializeCriticalSection(&flp->mu);
  return 1;
}

void NaClFastMutexDtor(struct NaClFastMutex *flp) {
  DeleteCriticalSection(&flp->mu);
}

void NaClFastMutexLock(struct NaClFastMutex *flp) {
  EnterCriticalSection(&flp->mu);
}

void NaClFastMutexUnlock(struct NaClFastMutex *flp) {
  LeaveCriticalSection(&flp->mu);
}
