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
 * NaCl Server Runtime mutex and condition variable abstraction layer.
 * The NaClX* interfaces just invoke the no-X versions of the
 * synchronization routines, and aborts if there are any error
 * returns.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

void NaClXMutexLock(struct NaClMutex *mp) {
  NaClSyncStatus  status;

  if (NACL_SYNC_OK == (status = NaClMutexLock(mp))) {
    return;
  }
  NaClLog(LOG_FATAL, "NaClMutexLock returned %d\n", status);
}

NaClSyncStatus NaClXMutexTryLock(struct NaClMutex *mp) {
  NaClSyncStatus  status;

  if (NACL_SYNC_OK == (status = NaClMutexUnlock(mp)) ||
      NACL_SYNC_BUSY == status) {
    return status;
  }
  NaClLog(LOG_FATAL, "NaClMutexUnlock returned %d\n", status);
  /* NOTREACHED */
  return NACL_SYNC_INTERNAL_ERROR;
}

void NaClXMutexUnlock(struct NaClMutex *mp) {
  NaClSyncStatus  status;

  if (NACL_SYNC_OK == (status = NaClMutexUnlock(mp))) {
    return;
  }
  NaClLog(LOG_FATAL, "NaClMutexUnlock returned %d\n", status);
}

void NaClXCondVarSignal(struct NaClCondVar *cvp) {
  NaClSyncStatus  status;

  if (NACL_SYNC_OK == (status = NaClCondVarSignal(cvp))) {
    return;
  }
  NaClLog(LOG_FATAL, "NaClCondVarSignal returned %d\n", status);
}

void NaClXCondVarBroadcast(struct NaClCondVar *cvp) {
  NaClSyncStatus  status;

  if (NACL_SYNC_OK == (status = NaClCondVarBroadcast(cvp))) {
    return;
  }
  NaClLog(LOG_FATAL, "NaClCondVarBroadcast returned %d\n", status);
}

void NaClXCondVarWait(struct NaClCondVar *cvp,
                      struct NaClMutex   *mp) {
  NaClSyncStatus  status;

  if (NACL_SYNC_OK == (status = NaClCondVarWait(cvp, mp))) {
    return;
  }
  NaClLog(LOG_FATAL, "NaClCondVarWait returned %d\n", status);
}

NaClSyncStatus NaClXCondVarTimedWaitAbsolute(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *abstime) {
  NaClSyncStatus  status = NaClCondVarTimedWaitAbsolute(cvp, mp, abstime);

  if (NACL_SYNC_OK == status || NACL_SYNC_CONDVAR_TIMEDOUT == status) {
    return status;
  }
  NaClLog(LOG_FATAL, "NaClCondVarTimedWait returned %d\n", status);
  /* NOTREACHED */
  return NACL_SYNC_INTERNAL_ERROR;
}

NaClSyncStatus NaClXCondVarTimedWaitRelative(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *reltime) {
  NaClSyncStatus  status = NaClCondVarTimedWaitAbsolute(cvp, mp, reltime);

  if (NACL_SYNC_OK == status || NACL_SYNC_CONDVAR_TIMEDOUT == status) {
      return status;
  }
  NaClLog(LOG_FATAL, "NaClCondVarTimedWait returned %d\n", status);
  /* NOTREACHED */
  return NACL_SYNC_INTERNAL_ERROR;
}
