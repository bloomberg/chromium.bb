/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_CUSTOM_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_CUSTOM_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"

EXTERN_C_BEGIN

struct NaClDescCustomFuncs {
  void (*Destroy)(void *handle);
  ssize_t (*SendMsg)(void *handle,
                     const struct NaClImcTypedMsgHdr *msg,
                     int flags);
  ssize_t (*RecvMsg)(void *handle,
                     struct NaClImcTypedMsgHdr *msg,
                     int flags);
};

#define NACL_DESC_CUSTOM_FUNCS_INITIALIZER {NULL, NULL, NULL}

/*
 * Example usage:
 *
 * struct NaClDescCustomFuncs funcs = NACL_DESC_CUSTOM_FUNCS_INITIALIZER;
 * funcs.Destroy = MyDescDestroy;
 * funcs.SendMsg = MyDescSendMsg;
 * funcs.RecvMsg = MyDescRecvMsg;
 * desc = NaClDescMakeCustomDesc(handle, &funcs);
 *
 * The purpose of the struct initializer is to allow new fields to be
 * added to the struct while keeping source compatibility.
 */
struct NaClDesc *NaClDescMakeCustomDesc(
    void *handle, const struct NaClDescCustomFuncs *funcs);

EXTERN_C_END

#endif
