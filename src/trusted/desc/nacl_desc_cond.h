/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescCondVar subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_COND_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_COND_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescCondVar;
struct NaClDescEffector;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescCondVarCtor(struct NaClDescCondVar  *self);

void NaClDescCondVarDtor(struct NaClDesc *vself);

int NaClDescCondVarClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp);

int NaClDescCondVarWait(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        struct NaClDesc         *mutex);

int NaClDescCondVarTimedWaitRel(struct NaClDesc           *vself,
                                struct NaClDescEffector   *effp,
                                struct NaClDesc           *mutex,
                                struct nacl_abi_timespec  *ts);

int NaClDescCondVarTimedWaitAbs(struct NaClDesc           *vself,
                                struct NaClDescEffector   *effp,
                                struct NaClDesc           *mutex,
                                struct nacl_abi_timespec  *ts);

int NaClDescCondVarSignal(struct NaClDesc         *vself,
                          struct NaClDescEffector *effp);

int NaClDescCondVarBroadcast(struct NaClDesc          *vself,
                             struct NaClDescEffector  *effp);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_COND_H_
