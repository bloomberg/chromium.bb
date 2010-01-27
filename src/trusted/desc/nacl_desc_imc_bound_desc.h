/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescImcBoundDesc subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IMC_BOUND_DESC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IMC_BOUND_DESC_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

/*
 * get NaClHandle, which is a typedef and not a struct pointer, so
 * impossible to just forward declare.
 */
#include "native_client/src/shared/imc/nacl_imc_c.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescImcBoundDesc;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescImcBoundDescCtor(struct NaClDescImcBoundDesc  *self,
                             NaClHandle                   d);

void NaClDescImcBoundDescDtor(struct NaClDesc *vself);

int NaClDescImcBoundDescClose(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp);

int NaClDescImcBoundDescAcceptConn(struct NaClDesc          *vself,
                                   struct NaClDescEffector  *effp);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_IMC_BOUND_DESC_H_
