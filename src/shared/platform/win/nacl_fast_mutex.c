/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/win/nacl_fast_mutex.h"

/*
 * Windows CRITICAL_SECTION objects are recursive locks.
 * NaClFastMutex should be binary mutexes.  Thus, we keep a flag
 * is_held to specify whether or not the lock is held.  Only the
 * thread holding the CRITICAL_SECTION could take the lock again via
 * EnterCriticalSection, so only that thread could observe a non-zero
 * value of is_held.  When that occurs, it is because of a recursive
 * lock acquisition; we crash the app when this occurs.
 *
 * If a CRITICAL_SECTION is abandoned, i.e., the thread holding it
 * exits, MSDN says that the state of the CRITICAL_SECTION is
 * undefined.  Ignoring standards/legalese interpretations that
 * include melting the CPU, reasonable implementation-defined behavior
 * include:
 *
 * - leaving the lock held, keeping all other threads from entering;
 *
 * - implicitly dropping the lock, letting another thread enter.
 *
 * In the first case, the application deadlocks.  This is fine (though
 * crashing would be better).  In the second case, we will see is_held
 * set and crash.  This is also reasonable.
 */

int NaClFastMutexCtor(struct NaClFastMutex *flp) {
  flp->is_held = 0;
  InitializeCriticalSection(&flp->mu);
  return 1;
}

void NaClFastMutexDtor(struct NaClFastMutex *flp) {
  CHECK(0 == flp->is_held);
  DeleteCriticalSection(&flp->mu);
}

void NaClFastMutexLock(struct NaClFastMutex *flp) {
  EnterCriticalSection(&flp->mu);
  CHECK(0 == flp->is_held);
  flp->is_held = 1;
}

void NaClFastMutexUnlock(struct NaClFastMutex *flp) {
  CHECK(1 == flp->is_held);
  flp->is_held = 0;
  LeaveCriticalSection(&flp->mu);
}
