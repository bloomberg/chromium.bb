/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A simple service for "kernel services".  The socket address will be
 * available to the NaCl module.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_KERNEL_SERVICE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_KERNEL_SERVICE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"

EXTERN_C_BEGIN

struct NaClRuntimeHostInterface;

struct NaClKernelService {
  struct NaClSimpleService        base NACL_IS_REFCOUNT_SUBCLASS;

  struct NaClRuntimeHostInterface *runtime_host;
};

int NaClKernelServiceCtor(
    struct NaClKernelService        *self,
    NaClThreadIfFactoryFunction     thread_factory_fn,
    void                            *thread_factory_data,
    struct NaClRuntimeHostInterface *runtime_host);

void NaClKernelServiceDtor(struct NaClRefCount *vself);

struct NaClKernelServiceVtbl {
  struct NaClSimpleServiceVtbl  vbase;

  void                          (*InitializationComplete)(
      struct NaClKernelService *self);

  /*
   * Create new service runtime process and return its socket address
   * in |out_sock_addr| argument. Returns 0 if successful or negative
   * ABI error value otherwise (see service_runtime/include/sys/errno.h)
   */
  int                           (*CreateProcess)(
      struct NaClKernelService *self,
      struct NaClDesc          **out_sock_addr,
      struct NaClDesc          **out_app_addr);
};

extern struct NaClKernelServiceVtbl const kNaClKernelServiceVtbl;

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_KERNEL_SERVICE_H_ */
