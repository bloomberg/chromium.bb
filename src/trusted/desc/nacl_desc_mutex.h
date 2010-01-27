/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescMutex subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_MUTEX_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_MUTEX_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescMutex;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescMutexCtor(struct NaClDescMutex  *self);

void NaClDescMutexDtor(struct NaClDesc *vself);

struct NaClDescMutex *NaClDescMutexMake(struct NaClIntrMutex *mu);

int NaClDescMutexClose(struct NaClDesc          *vself,
                       struct NaClDescEffector  *effp);

int NaClDescMutexLock(struct NaClDesc         *vself,
                      struct NaClDescEffector *effp);

int NaClDescMutexTryLock(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp);

int NaClDescMutexUnlock(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_MUTEX_H_
