/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime mutex and condition variable abstraction layer.
 * The NaClX* interfaces just invoke the no-X versions of the
 * synchronization routines, and aborts if there are any error
 * returns.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SYNC_CHECKED_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SYNC_CHECKED_H_

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/shared/platform/nacl_sync.h"

EXTERN_C_BEGIN

/*
 * These are simple checked versions of the nacl_sync API which will
 * abort on any unexpected return value.
 */

void NaClXMutexLock(struct NaClMutex *mp);
NaClSyncStatus NaClXMutexTryLock(struct NaClMutex *mp);
void NaClXMutexUnlock(struct NaClMutex *mp);

void NaClXCondVarSignal(struct NaClCondVar *cvp);
void NaClXCondVarBroadcast(struct NaClCondVar *cvp);
void NaClXCondVarWait(struct NaClCondVar *cvp,
                      struct NaClMutex   *mp);

NaClSyncStatus NaClXCondVarTimedWaitAbsolute(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *abstime);

NaClSyncStatus NaClXCondVarTimedWaitRelative(
    struct NaClCondVar              *cvp,
    struct NaClMutex                *mp,
    struct nacl_abi_timespec const  *reltime);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_SYNC_CHECKED_H_ */
