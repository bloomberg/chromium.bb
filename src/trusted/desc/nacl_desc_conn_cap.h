/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescConnCap subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_CONN_CAP_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_CONN_CAP_H_

#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescConnCap;
struct NaClDescEffector;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct NaClSocketAddress;
struct nacl_abi_stat;
struct nacl_abi_timespec;

int NaClDescConnCapCtor(struct NaClDescConnCap          *self,
                        struct NaClSocketAddress const  *nsap);

void NaClDescConnCapDtor(struct NaClDesc *vself);

int NaClDescConnCapClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp);

int NaClDescConnCapExternalizeSize(struct NaClDesc  *vself,
                                   size_t           *nbytes,
                                   size_t           *nhandles);

int NaClDescConnCapExternalize(struct NaClDesc          *vself,
                               struct NaClDescXferState *xfer);

int NaClDescConnCapConnectAddr(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp);

int NaClDescConnCapAcceptConn(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_CONN_CAP_H_
