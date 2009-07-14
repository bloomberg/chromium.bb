/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * C bindings for C++ implementation of synchronization objects
 * (based on Crome code)
 */

#include "native_client/src/include/portability.h"

#include <sys/types.h>
#include <sys/timeb.h>

#include "native_client/src/trusted/platform/nacl_sync.h"

#if NACL_WINDOWS
# include "native_client/src/trusted/platform/win/lock.h"
# include "native_client/src/trusted/platform/win/condition_variable.h"
#elif NACL_LINUX || NACL_OSX
# include "native_client/src/trusted/platform/linux/lock.h"
# include "native_client/src/trusted/platform/linux/condition_variable.h"
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
