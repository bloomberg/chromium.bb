/*
 * Copyright 2011 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SIMPLE_SERVICE_LTD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SIMPLE_SERVICE_LTD_H_

/*
 * Subclass of NaClSimpleService, with a limited number of
 * clients/connections so the amount of resources (threads) used by
 * the service cannot become unbounded.
 */


#include "native_client/src/include/nacl_base.h"

#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/service_runtime/nacl_simple_service.h"

EXTERN_C_BEGIN

struct NaClSimpleLtdService {
  struct NaClSimpleService  base;
  struct NaClMutex          mu;
  struct NaClCondVar        cv;
  size_t                    max_clients;
  size_t                    num_clients;
};

int NaClSimpleLtdServiceCtor(struct NaClSimpleLtdService        *self,
                             struct NaClSrpcHandlerDesc const   *srpc_handlers,
                             int                                max_cli,
                             void                               *instance_data);

/*
 * Wait until num_client < max_client, then drop locks and accept as
 * base class behavior.
 */
int NaClSimpleLtdServiceAcceptConnection(
    struct NaClSimpleService            *vself,
    struct NaClSimpleServiceConnection  **out);

/*
 * Base class behavior, but before returning first decrement
 * num_client and broadcast the condition update.
 */
void NaClSimpleLtdServiceRpcHandler(
    struct NaClSimpleService            *vself,
    struct NaClSimpleServiceConnection  *conn);

extern struct NaClSimpleServiceVtbl const kNaClSimpleLtdServiceVtbl;

EXTERN_C_END

#endif
