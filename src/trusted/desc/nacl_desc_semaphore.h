/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescSemaphore subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SEMAPHORE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SEMAPHORE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClHostDesc;
struct NaClDescXferState;
struct NaClDescSemaphore;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescSemaphoreCtor(struct NaClDescSemaphore  *self, int value);

void NaClDescSemaphoreDtor(struct NaClDesc *vself);

int NaClDescSemaphoreClose(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp);

int NaClDescSemaphorePost(struct NaClDesc         *vself,
                          struct NaClDescEffector *effp);

int NaClDescSemaphoreSemWait(struct NaClDesc          *vself,
                             struct NaClDescEffector  *effp);

int NaClDescSemaphoreGetValue(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SEMAPHORE_H_
