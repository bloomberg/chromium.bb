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
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"


struct NaClSrpcHandlerDesc const kNaClKernelServiceHandlers[];

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

void NaClKernelServiceInitializationComplete(
    struct NaClKernelService *self) {
  struct NaClApp  *nap;
  NaClSrpcError   rpc_result;

  NaClLog(4,
          "NaClKernelServiceInitializationComplete(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) self);

  nap = self->nap;
  NaClXMutexLock(&nap->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      nap->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&nap->reverse_channel,
                                           NACL_REVERSE_CONTROL_INIT_DONE);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              ("NaClKernelServiceInitializationComplete: "
               "init_done RPC failed: %d\n"),
              rpc_result);
    }
  } else {
    NaClLog(3, "NaClKernelServiceInitializationComplete: no reverse channel"
            ", no plugin to talk to.\n");
  }
  NaClXMutexUnlock(&nap->mu);
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

int NaClKernelServiceCreateProcess(
    struct NaClKernelService   *self,
    struct NaClDesc            **out_sock_addr) {
  struct NaClApp  *nap;
  NaClSrpcError   rpc_result;
  int             status = 0;

  NaClLog(4,
          ("NaClKernelServiceCreateProcess(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR"\n"),
          (uintptr_t) self, (uintptr_t) out_sock_addr);

  nap = self->nap;
  NaClXMutexLock(&nap->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      nap->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&nap->reverse_channel,
                                           NACL_REVERSE_CONTROL_CREATE_PROCESS,
                                           &status, out_sock_addr);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              "NaClKernelServiceCreateProcess: RPC failed, result %d\n",
              rpc_result);
    }
  } else {
    NaClLog(3, "NaClKernelServiceCreateProcess: no reverse channel"
            ", no plugin to talk to.\n");
    status = -NACL_ABI_EAGAIN;
  }
  NaClXMutexUnlock(&nap->mu);

  return status;
}

static void NaClKernelServiceCreateProcessRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClKernelService  *nksp =
    (struct NaClKernelService *) rpc->channel->server_instance_data;
  int                       status;
  struct NaClDesc           *desc;
  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4, "NaClKernelServiceCreateProcessRpc: creating process\n");
  status = (*NACL_VTBL(NaClKernelService, nksp)->CreateProcess)(
      nksp, &desc);
  out_args[0]->u.ival = status;
  out_args[1]->u.hval = (0 == status)
      ? desc
      : (struct NaClDesc *) NaClDescInvalidMake();
  NaClLog(3, "NaClKernelServiceCreateProcessRpc: status %d\n", status);
  NaClLog(3, "NaClKernelServiceCreateProcessRpc: desc 0x08%"NACL_PRIxPTR"\n",
          (uintptr_t) desc);
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

struct NaClSrpcHandlerDesc const kNaClKernelServiceHandlers[] = {
  { NACL_KERNEL_SERVICE_INITIALIZATION_COMPLETE,
    NaClKernelServiceInitializationCompleteRpc, },
  { NACL_KERNEL_SERVICE_CREATE_PROCESS,
    NaClKernelServiceCreateProcessRpc, },
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
  NaClKernelServiceCreateProcess,
};
