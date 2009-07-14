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
 * This is the host-OS-independent interface.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SYNC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SYNC_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"

EXTERN_C_BEGIN

struct NaClMutex {
  void* lock;
};

struct NaClCondVar {
  void* cv;
};

typedef enum NaClSyncStatus {
  NACL_SYNC_OK,
  NACL_SYNC_INTERNAL_ERROR,
  NACL_SYNC_BUSY,
  NACL_SYNC_MUTEX_INVALID,
  NACL_SYNC_MUTEX_DEADLOCK,
  NACL_SYNC_MUTEX_PERMISSION,
  NACL_SYNC_MUTEX_INTERRUPTED,
  NACL_SYNC_CONDVAR_TIMEDOUT,
  NACL_SYNC_CONDVAR_INTR,
  NACL_SYNC_SEM_INTERRUPTED,
  NACL_SYNC_SEM_RANGE_ERROR
} NaClSyncStatus;

int NaClMutexCtor(struct NaClMutex *mp);  /* bool success/fail */

void NaClMutexDtor(struct NaClMutex *mp);

NaClSyncStatus NaClMutexLock(struct NaClMutex *mp);

NaClSyncStatus NaClMutexTryLock(struct NaClMutex *mp);

NaClSyncStatus NaClMutexUnlock(struct NaClMutex *mp);


int NaClCondVarCtor(struct NaClCondVar *cvp);

void NaClCondVarDtor(struct NaClCondVar *cvp);

NaClSyncStatus NaClCondVarSignal(struct NaClCondVar *cvp);

NaClSyncStatus NaClCondVarBroadcast(struct NaClCondVar *cvp);

NaClSyncStatus NaClCondVarWait(struct NaClCondVar *cvp,
                               struct NaClMutex   *mp);

NaClSyncStatus NaClCondVarTimedWaitRelative(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *reltime);

NaClSyncStatus NaClCondVarTimedWaitAbsolute(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *abstime);



EXTERN_C_END


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SYNC_H_ */
