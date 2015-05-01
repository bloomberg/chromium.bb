/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_secure_service.h"

#include <string.h>

#include "native_client/src/public/secure_service.h"

#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/fault_injection/fault_injection.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

struct NaClSrpcHandlerDesc const kNaClSecureServiceHandlers[];

int NaClSecureServiceCtor(struct NaClSecureService  *self,
                          struct NaClApp            *nap,
                          struct NaClDesc           *service_port,
                          struct NaClDesc           *sock_addr) {
  NaClLog(4,
          "Entered NaClSecureServiceCtor: self 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  if (NACL_FI_ERROR_COND(
          "NaClSecureServiceCtor__NaClSimpleServiceWithSocketCtor",
          !NaClSimpleServiceWithSocketCtor(
              &self->base,
              kNaClSecureServiceHandlers,
              NaClThreadInterfaceThreadFactory,
              (void *) self,
              service_port,
              sock_addr))) {
    goto done;
  }
  if (!NaClMutexCtor(&self->mu)) {
    NaClLog(4, "NaClMutexCtor failed\n");
    goto failure_mutex_ctor;
  }
  if (!NaClCondVarCtor(&self->cv)) {
    NaClLog(4, "NaClCondVar failed\n");
    goto failure_condvar_ctor;
  }
  NaClXMutexCtor(&self->mu);
  NaClXCondVarCtor(&self->cv);
  self->nap = nap;
  self->conn_count = 0;
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSecureServiceVtbl;
  return 1;

 failure_condvar_ctor:
  NaClMutexDtor(&self->mu);
 failure_mutex_ctor:
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
 done:
  return 0;
}

void NaClSecureServiceDtor(struct NaClRefCount *vself) {
  struct NaClSecureService *self = (struct NaClSecureService *) vself;

  NaClXMutexLock(&self->mu);
  if (0 != self->conn_count) {
    NaClLog(LOG_FATAL,
            "SecureService dtor when connection count is nonzero\n");
  }
  self->conn_count = 0;
  NaClXMutexUnlock(&self->mu);

  NaClCondVarDtor(&self->cv);
  NaClMutexDtor(&self->mu);

  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl const *)
      &kNaClSimpleServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClSecureServiceConnectionFactory(
    struct NaClSimpleService            *vself,
    struct NaClDesc                     *conn,
    struct NaClSimpleServiceConnection  **out) {
  /* our instance_data is not connection specific */
  return NaClSimpleServiceConnectionFactoryWithInstanceData(
      vself, conn, (struct NaClSecureService *) vself, out);
}

static void NaClSecureServiceConnectionCountIncr(
    struct NaClSecureService *self) {
  NaClLog(5, "NaClSecureServiceThreadCountIncr\n");
  NaClXMutexLock(&self->mu);
  if (0 == ++self->conn_count) {
    NaClLog(LOG_FATAL,
            "NaClSecureServiceThreadCountIncr: "
            "thread count overflow!\n");
  }
  NaClXMutexUnlock(&self->mu);
}

static void NaClSecureServiceConnectionCountDecr(
    struct NaClSecureService *self) {
  uint32_t conn_count;

  NaClLog(5, "NaClSecureServiceThreadCountDecr\n");
  NaClXMutexLock(&self->mu);
  if (0 == self->conn_count) {
    NaClLog(LOG_FATAL,
            "NaClSecureServiceThreadCountDecr: "
            "decrementing thread count when count is zero\n");
  }
  conn_count = --self->conn_count;
  NaClXMutexUnlock(&self->mu);

  if (0 == conn_count) {
    NaClLog(4, "NaClSecureServiceThread: all channels closed, exiting.\n");
    /*
     * Set that we are killed by SIGKILL so that debug stub could report
     * this to debugger.
     */
    NaClAppShutdown(self->nap, NACL_ABI_W_EXITCODE(0, NACL_ABI_SIGKILL));
  }
}

int NaClSecureServiceAcceptConnection(
    struct NaClSimpleService            *vself,
    struct NaClSimpleServiceConnection  **vconn) {
  struct NaClSecureService *self =
    (struct NaClSecureService *) vself;
  int status;

  NaClLog(4, "NaClSecureServiceAcceptConnection\n");
  status = (*kNaClSimpleServiceVtbl.AcceptConnection)(vself, vconn);
  if (0 == status) {
    NaClSecureServiceConnectionCountIncr(self);
  }
  NaClLog(4, "Leaving NaClSecureServiceAcceptConnection, status %d.\n", status);
  return status;
}

void NaClSecureServiceRpcHandler(struct NaClSimpleService           *vself,
                                 struct NaClSimpleServiceConnection *vconn) {
  struct NaClSecureService *self =
    (struct NaClSecureService *) vself;

  NaClLog(4, "NaClSecureChannelThread started\n");
  (*kNaClSimpleServiceVtbl.RpcHandler)(vself, vconn);
  NaClLog(4, "NaClSecureChannelThread closed.\n");
  NaClSecureServiceConnectionCountDecr(self);
}

static void NaClSecureServiceLoadModuleRpcCallback(
    void            *instance_data,
    NaClErrorCode   status) {
  struct NaClSrpcClosure *done_cls =
      (struct NaClSrpcClosure *) instance_data;
  UNREFERENCED_PARAMETER(status);
  NaClLog(4, "NaClSecureChannelLoadModuleRpcCallback: status %d\n", status);
  (*done_cls->Run)(done_cls);
}

/* TODO(teravest): Remove this once http://crbug.com/333950 is resolved. */
static void NaClSecureServiceLoadModuleRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClSecureService        *nssp =
      (struct NaClSecureService *) rpc->channel->server_instance_data;
  struct NaClDesc                 *nexe = in_args[0]->u.hval;
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(4, "NaClSecureServiceLoadModuleRpc: loading module\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  NaClAppLoadModule(nssp->nap,
                    nexe,
                    NaClSecureServiceLoadModuleRpcCallback,
                    (void *) done_cls);
  NaClDescUnref(nexe);
  nexe = NULL;

  NaClLog(4, "NaClSecureServiceLoadModuleRpc: done\n");
}

struct StartModuleCallbackState {
  int                    *out_status;
  struct NaClSrpcClosure *cls;
};

static void NaClSecureServiceStartModuleRpcCallback(
    void            *instance_data,
    NaClErrorCode   status) {
  struct StartModuleCallbackState *state =
      (struct StartModuleCallbackState *) instance_data;
  NaClLog(4, "NaClSecureChannelStartModuleRpcCallback: status %d\n", status);

  /*
   * The RPC reply is now sent.  This has to occur before we signal
   * the main thread to possibly start, since in the case of a failure
   * the main thread may quickly exit.  If the main thread does this
   * before we sent the RPC reply, then the plugin will be left
   * without an answer.
   */
  *state->out_status = (int) status;
  (*state->cls->Run)(state->cls);
  free(state);
}

static void NaClSecureServiceStartModuleRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClSecureService        *nssp =
      (struct NaClSecureService *) rpc->channel->server_instance_data;
  struct StartModuleCallbackState *state;
  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4, "NaClSecureChannelStartModuleRpc: starting module\n");

  state = (struct StartModuleCallbackState *) malloc(sizeof *state);
  if (NULL == state) {
    rpc->result = NACL_SRPC_RESULT_NO_MEMORY;
    (*done_cls->Run)(done_cls);
    return;
  }
  state->out_status = &out_args[0]->u.ival;
  state->cls = done_cls;

  rpc->result = NACL_SRPC_RESULT_OK;
  NaClAppStartModule(nssp->nap,
                     NaClSecureServiceStartModuleRpcCallback,
                     (void *) state);

  NaClLog(4, "NaClSecureChannelStartModuleRpc: done\n");
}

static void NaClSecureServiceShutdownRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done) {
  struct NaClSecureService  *nssp =
      (struct NaClSecureService *) rpc->channel->server_instance_data;
  UNREFERENCED_PARAMETER(rpc);
  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);
  UNREFERENCED_PARAMETER(done);

  NaClAppShutdown(nssp->nap, 0);
}

struct NaClSrpcHandlerDesc const kNaClSecureServiceHandlers[] = {
  { NACL_SECURE_SERVICE_LOAD_MODULE, NaClSecureServiceLoadModuleRpc, },
  { NACL_SECURE_SERVICE_START_MODULE, NaClSecureServiceStartModuleRpc, },
  { NACL_SECURE_SERVICE_HARD_SHUTDOWN, NaClSecureServiceShutdownRpc, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};

struct NaClSimpleServiceVtbl const kNaClSecureServiceVtbl = {
  {
    NaClSecureServiceDtor,
  },
  NaClSecureServiceConnectionFactory,
  NaClSecureServiceAcceptConnection,
  NaClSimpleServiceAcceptAndSpawnHandler,
  NaClSecureServiceRpcHandler,
};
