/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_REVERSE_HOST_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_REVERSE_HOST_INTERFACE_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/service_runtime/nacl_runtime_host_interface.h"

EXTERN_C_BEGIN

struct NaClSecureService;

struct NaClReverseHostInterface {
  struct NaClRuntimeHostInterface   base NACL_IS_REFCOUNT_SUBCLASS;

  struct NaClSecureService          *server;
};

int NaClReverseHostInterfaceCtor(
    struct NaClReverseHostInterface *self,
    struct NaClSecureService        *server);

void NaClReverseHostInterfaceDtor(struct NaClRefCount *vself);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_REVERSE_HOST_INTERFACE_H_ */
