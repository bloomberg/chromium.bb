/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * C bindings for C++ implementation of synchronization objects
 * (based on Chrome code)
 */

#ifndef __native_client__
/* Beware: there is a delicate requirement here that the untrusted code be able
 * to include nacl_abi_* type definitions.  These headers share an include
 * guard with the exported versions, which get included first.  Unfortunately,
 * tying them to different include guards causes multiple definitions of
 * macros.
 */
#include "native_client/src/include/portability.h"
#include <sys/types.h>
#include <sys/timeb.h>
#endif  /* __native_client__ */

#include "native_client/src/shared/platform/nacl_sync.h"

#if NACL_WINDOWS
# include "native_client/src/shared/platform/win/condition_variable.h"
# include "native_client/src/shared/platform/win/lock.h"
#elif NACL_LINUX || NACL_OSX || defined(__native_client__)
# include "native_client/src/shared/platform/linux/condition_variable.h"
# include "native_client/src/shared/platform/linux/lock.h"
#endif

/* Mutex */
int NaClMutexCtor(struct NaClMutex *mp) {
  mp->lock = new NaCl::Lock();
  return 1;
}

void NaClMutexDtor(struct NaClMutex *mp) {
  delete reinterpret_cast<NaCl::Lock *>(mp->lock);
  mp->lock = NULL;
}

NaClSyncStatus NaClMutexLock(struct NaClMutex *mp) {
  /* TODO(gregoryd) - add argument validation for debug version,
   * here and below
   */
  reinterpret_cast<NaCl::Lock *>(mp->lock)->Acquire();
  return NACL_SYNC_OK;
}

NaClSyncStatus NaClMutexTryLock(struct NaClMutex *mp) {
  return reinterpret_cast<NaCl::Lock *>(mp->lock)->Try() ? NACL_SYNC_OK :
      NACL_SYNC_BUSY;
}

NaClSyncStatus NaClMutexUnlock(struct NaClMutex *mp) {
  reinterpret_cast<NaCl::Lock *>(mp->lock)->Release();
  return NACL_SYNC_OK;
}

/* Condition variable */

int NaClCondVarCtor(struct NaClCondVar  *cvp) {
  cvp->cv = new NaCl::ConditionVariable();
  return 1;
}

void NaClCondVarDtor(struct NaClCondVar *cvp) {
  delete reinterpret_cast<NaCl::ConditionVariable*>(cvp->cv);
}

NaClSyncStatus NaClCondVarSignal(struct NaClCondVar *cvp) {
  reinterpret_cast<NaCl::ConditionVariable*>(cvp->cv)->Signal();
  return NACL_SYNC_OK;
}

NaClSyncStatus NaClCondVarBroadcast(struct NaClCondVar *cvp) {
  reinterpret_cast<NaCl::ConditionVariable*>(cvp->cv)->Broadcast();
  return NACL_SYNC_OK;
}

NaClSyncStatus NaClCondVarWait(struct NaClCondVar *cvp,
                               struct NaClMutex   *mp) {
  reinterpret_cast<NaCl::ConditionVariable*>(cvp->cv)->Wait(
      *(reinterpret_cast<NaCl::Lock *>(mp->lock)));
  return NACL_SYNC_OK;
}

NaClSyncStatus NaClCondVarTimedWaitRelative(
    struct NaClCondVar             *cvp,
    struct NaClMutex               *mp,
    struct nacl_abi_timespec const *reltime) {
  NaCl::TimeDelta relative_wait(
      NaCl::TimeDelta::FromMicroseconds(
          reltime->tv_sec
          * NaCl::Time::kMicrosecondsPerSecond
          + reltime->tv_nsec
          / NaCl::Time::kNanosecondsPerMicrosecond));
  int result;
  result = (reinterpret_cast<NaCl::ConditionVariable*>(cvp->cv)->TimedWaitRel(
                *(reinterpret_cast<NaCl::Lock *>(mp->lock)),
      relative_wait));
  if (0 == result) {
    return NACL_SYNC_CONDVAR_TIMEDOUT;
  }
  return NACL_SYNC_OK;
}

NaClSyncStatus NaClCondVarTimedWaitAbsolute(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *abstime) {
  NaCl::TimeTicks ticks(abstime->tv_sec
                        * NaCl::Time::kMicrosecondsPerSecond
                        + abstime->tv_nsec
                        / NaCl::Time::kNanosecondsPerMicrosecond);
  int result;
  result = reinterpret_cast<NaCl::ConditionVariable*>(cvp->cv)->TimedWaitAbs(
      *(reinterpret_cast<NaCl::Lock *>(mp->lock)),
      ticks);
  if (0 == result) {
    return NACL_SYNC_CONDVAR_TIMEDOUT;
  }
  return NACL_SYNC_OK;
}
