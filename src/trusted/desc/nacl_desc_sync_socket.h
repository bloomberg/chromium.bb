/*
 * Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescSyncSocket subclass of NaClDesc.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SYNC_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SYNC_SOCKET_H_

#include "native_client/src/include/portability.h"

#include "native_client/src/include/nacl_base.h"

/*
 * get NaClHandle, which is a typedef and not a struct pointer, so
 * impossible to just forward declare.
 */
#include "native_client/src/shared/imc/nacl_imc_c.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffector;
struct NaClDescSyncSocket;
struct NaClDescXferState;
struct NaClHostDesc;
struct NaClMessageHeader;
struct nacl_abi_timespec;
struct nacl_abi_stat;

/* On success, NaClDescSyncSocket has ownership of h. */
int NaClDescSyncSocketCtor(struct NaClDescSyncSocket  *self,
                           NaClHandle                 h);

int NaClDescSyncSocketClose(struct NaClDesc          *vself,
                            struct NaClDescEffector  *effp);

int NaClDescSyncSocketExternalizeSize(struct NaClDesc  *vself,
                                      size_t           *nbytes,
                                      size_t           *nhandles);

int NaClDescSyncSocketExternalize(struct NaClDesc          *vself,
                                  struct NaClDescXferState *xfer);

int NaClDescSyncSocketSendMsg(struct NaClDesc          *vself,
                              struct NaClDescEffector  *effp,
                              struct NaClMessageHeader *dgram,
                              int                      flags);

int NaClDescSyncSocketRecvMsg(struct NaClDesc          *vself,
                              struct NaClDescEffector  *effp,
                              struct NaClMessageHeader *dgram,
                              int                      flags);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_SYNC_SOCKET_H_
