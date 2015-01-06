/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A simple service for "kernel services".  The socket address will be
 * available to the NaCl module.
 */

#include "native_client/src/trusted/service_runtime/include/sys/nacl_kernel_service.h"
#include "native_client/src/trusted/service_runtime/nacl_kernel_service.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/nacl_runtime_host_interface.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"


struct NaClSrpcHandlerDesc const kNaClKernelServiceHandlers[];

int NaClKernelServiceCtor(
    struct NaClKernelService        *self,
    NaClThreadIfFactoryFunction     thread_factory_fn,
    void                            *thread_factory_data,
    struct NaClRuntimeHostInterface *runtime_host) {
  NaClLog(4,
          ("NaClKernelServiceCtor: self 0x%"NACL_PRIxPTR","
           " runtime_host 0x%"NACL_PRIxPTR"\n"),
          (uintptr_t) self,
          (uintptr_t) runtime_host);
  if (!NaClSimpleServiceCtor(&self->base,
                             kNaClKernelServiceHandlers,
                             thread_factory_fn,
                             thread_factory_data)) {
    return 0;
  }
  self->runtime_host = (struct NaClRuntimeHostInterface *)
      NaClRefCountRef((struct NaClRefCount *) runtime_host);
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClKernelServiceVtbl;
  return 1;
}

void NaClKernelServiceDtor(struct NaClRefCount *vself) {
  struct NaClKernelService *self = (struct NaClKernelService *) vself;

  NaClRefCountUnref((struct NaClRefCount *) self->runtime_host);

  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSimpleServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

void NaClKernelServiceInitializationComplete(
    struct NaClKernelService *self) {
  NaClLog(4,
          "NaClKernelServiceInitializationComplete(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) self);

  (*NACL_VTBL(NaClRuntimeHostInterface, self->runtime_host)->
    StartupInitializationComplete)(self->runtime_host);
}

static void NaClKernelServiceInitializationCompleteRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClKernelService  *nksp =
    (struct NaClKernelService *) rpc->channel->server_instance_data;
  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);

  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);

  (*NACL_VTBL(NaClKernelService, nksp)->InitializationComplete)(nksp);
}

struct NaClSrpcHandlerDesc const kNaClKernelServiceHandlers[] = {
  { NACL_KERNEL_SERVICE_INITIALIZATION_COMPLETE,
    NaClKernelServiceInitializationCompleteRpc, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};

struct NaClKernelServiceVtbl const kNaClKernelServiceVtbl = {
  {
    {
      NaClKernelServiceDtor,
    },
    NaClSimpleServiceConnectionFactory,
    NaClSimpleServiceAcceptConnection,
    NaClSimpleServiceAcceptAndSpawnHandler,
    NaClSimpleServiceRpcHandler,
  },
  NaClKernelServiceInitializationComplete,
};
