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

#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

static void NaClKernelServiceInitializationCompleteRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClKernelService  *service =
      (struct NaClKernelService *) rpc->channel->server_instance_data;
  struct NaClApp            *nap;
  NaClSrpcError             rpc_result;

  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);

  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);

  NaClLog(4,
          "NaClKernelServiceInitializationCompleteRpc: nap 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) service->nap);
  nap = service->nap;
  NaClXMutexLock(&nap->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      nap->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&nap->reverse_channel,
                                           NACL_REVERSE_CONTROL_INIT_DONE);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              "NaClKernelService: InitDone RPC failed: %d\n", rpc_result);
    }
  } else {
    NaClLog(3, "NaClKernelService: no reverse channel, no plugin to talk to.\n");
  }
  NaClXMutexUnlock(&nap->mu);
}

struct NaClSrpcHandlerDesc const kNaClKernelServiceHandlers[] = {
  { NACL_KERNEL_SERVICE_INITIALIZATION_COMPLETE,
    NaClKernelServiceInitializationCompleteRpc, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};

int NaClKernelServiceCtor(
    struct NaClKernelService      *self,
    NaClThreadIfFactoryFunction   thread_factory_fn,
    void                          *thread_factory_data,
    struct NaClApp                *nap) {
  NaClLog(4,
          ("NaClKernelServiceCtor: self 0x%"NACL_PRIxPTR", nap 0x%"
           NACL_PRIxPTR"\n"),
          (uintptr_t) self,
          (uintptr_t) nap);
  if (!NaClSimpleServiceCtor(&self->base,
                             kNaClKernelServiceHandlers,
                             thread_factory_fn,
                             thread_factory_data)) {
    return 0;
  }
  self->nap = nap;
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClKernelServiceVtbl;
  return 1;
}

void NaClKernelServiceDtor(struct NaClRefCount *vself) {
  struct NaClKernelService *self = (struct NaClKernelService *) vself;
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSimpleServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

struct NaClSimpleServiceVtbl const kNaClKernelServiceVtbl = {
  {
    NaClKernelServiceDtor,
  },
  NaClSimpleServiceConnectionFactory,
  NaClSimpleServiceAcceptConnection,
  NaClSimpleServiceAcceptAndSpawnHandler,
  NaClSimpleServiceRpcHandler,
};
