/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime mutex and condition variable abstraction layer.
 * This is the host-OS-independent interface.
 */
#ifndef NATIVE_CLIENT_SRC_SHARED_PLATFORM_NACL_SYNC_H_
#define NATIVE_CLIENT_SRC_SHARED_PLATFORM_NACL_SYNC_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"

EXTERN_C_BEGIN

struct NaClMutex {
  void* lock;
};

struct NaClCondVar {
  void* cv;
};

struct nacl_abi_timespec;

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


#endif  /* NATIVE_CLIENT_SRC_SHARED_PLATFORM_NACL_SYNC_H_ */
