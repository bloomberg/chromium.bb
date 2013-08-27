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
#include "native_client/src/trusted/manifest_name_service_proxy/manifest_proxy.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/nacl_reverse_host_interface.h"
#include "native_client/src/trusted/service_runtime/nacl_reverse_quota_interface.h"
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
  self->reverse_channel_initialization_state =
      NACL_REVERSE_CHANNEL_UNINITIALIZED;
  self->reverse_client = NULL;
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
  if (NACL_REVERSE_CHANNEL_UNINITIALIZED !=
      self->reverse_channel_initialization_state) {
    while (NACL_REVERSE_CHANNEL_INITIALIZED !=
           self->reverse_channel_initialization_state) {
      NaClXCondVarWait(&self->cv, &self->mu);
    }
  }

  if (0 != self->conn_count) {
    NaClLog(LOG_FATAL,
            "SecureService dtor when connection count is nonzero\n");
  }
  self->conn_count = 0;

  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      self->reverse_channel_initialization_state) {
    NaClSrpcDtor(&self->reverse_channel);
  }
  if (NULL != self->reverse_client) {
    NaClRefCountUnref((struct NaClRefCount *) self->reverse_client);
  }
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

static void NaClSecureServiceLoadModuleRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClSecureService        *nssp =
      (struct NaClSecureService *) rpc->channel->server_instance_data;
  struct NaClDesc                 *nexe = in_args[0]->u.hval;
  char                            *aux_info;
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(4, "NaClSecureServiceLoadModuleRpc: loading module\n");

  rpc->result = NACL_SRPC_RESULT_INTERNAL;

  aux_info = strdup(in_args[1]->arrays.str);
  if (NULL == aux_info) {
    rpc->result = NACL_SRPC_RESULT_NO_MEMORY;
    (*done_cls->Run)(done_cls);
    goto cleanup;
  }

  rpc->result = NACL_SRPC_RESULT_OK;
  NaClAppLoadModule(nssp->nap,
                    nexe,
                    aux_info,
                    NaClSecureServiceLoadModuleRpcCallback,
                    (void *) done_cls);

 cleanup:
  NaClLog(4, "NaClSecureServiceLoadModuleRpc: done\n");
  NaClDescUnref(nexe);
}

/*
 * The first connection is performed by this callback handler.  This
 * spawns a client thread that will bootstrap the other connections by
 * stashing the connection represented by |conn| to make reverse RPCs
 * to ask the peer to connect to us.  No thread is spawned; we just
 * wrap access to the connection with a lock.
 *
 * Subsequent connection callbacks will pass the connection to the
 * actual thread that made the connection request using |conn|
 * received in the first connection.
 */
static void NaClSecureReverseClientCallback(
    void                        *state,
    struct NaClThreadInterface  *tif,
    struct NaClDesc             *new_conn) {
  struct NaClSecureService          *self =
      (struct NaClSecureService *) state;
  struct NaClApp                    *nap = self->nap;
  struct NaClManifestProxy          *manifest_proxy;
  struct NaClReverseHostInterface   *reverse_host_interface;
  struct NaClReverseQuotaInterface  *reverse_quota_interface;
  UNREFERENCED_PARAMETER(tif);

  NaClLog(4,
          ("Entered NaClSecureReverseClientCallback: self 0x%"NACL_PRIxPTR","
           " nap 0x%"NACL_PRIxPTR", new_conn 0x%"NACL_PRIxPTR"\n"),
          (uintptr_t) self, (uintptr_t) nap, (uintptr_t) new_conn);

  NaClXMutexLock(&self->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZATING !=
      self->reverse_channel_initialization_state) {
    /*
     * The reverse channel connection capability is used to make the
     * RPC that invokes this callback (this callback is invoked on a
     * reverse channel connect), so the plugin wants to initialize the
     * reverse channel and in particular the state must be either be
     * in-progress or finished.
     */
    NaClLog(LOG_FATAL, "Reverse channel already initialized\n");
  }
  NaClXMutexUnlock(&self->mu);
  if (!NaClSrpcClientCtor(&self->reverse_channel, new_conn)) {
    NaClLog(LOG_FATAL, "Reverse channel SRPC Client Ctor failed\n");
    goto done;
  }
  reverse_host_interface = (struct NaClReverseHostInterface *)
    malloc(sizeof *reverse_host_interface);
  if (NULL == reverse_host_interface ||
      NACL_FI_ERROR_COND(
          ("NaClSecureReverseClientCallback"
           "__NaClReverseInterfaceCtor"),
          !NaClReverseHostInterfaceCtor(reverse_host_interface,
                                        self))) {
    NaClLog(LOG_FATAL, "Reverse interface ctor failed\n");
    goto cleanup_reverse_host_interface;
  }
  NaClAppRuntimeHostSetup(nap, (struct NaClRuntimeHostInterface *)
      reverse_host_interface);

  reverse_quota_interface = (struct NaClReverseQuotaInterface *)
    malloc(sizeof *reverse_quota_interface);
  if (NULL == reverse_quota_interface ||
      NACL_FI_ERROR_COND(
          ("NaClSecureReverseClientCallback"
           "__NaClReverseQuotaInterfaceCtor"),
          !NaClReverseQuotaInterfaceCtor(reverse_quota_interface,
                                         self))) {
    NaClLog(LOG_FATAL, "Reverse quota interface ctor failed\n");
    goto cleanup_reverse_quota_interface;
  }
  NaClAppDescQuotaSetup(nap, (struct NaClDescQuotaInterface *)
      reverse_quota_interface);

  NaClRefCountSafeUnref((struct NaClRefCount *) reverse_quota_interface);
  reverse_quota_interface = NULL;

  manifest_proxy = (struct NaClManifestProxy *)
      malloc(sizeof *manifest_proxy);
  if (NULL == manifest_proxy ||
      !NaClManifestProxyCtor(manifest_proxy,
                             NaClAddrSpSquattingThreadIfFactoryFunction,
                             (void *) nap,
                             self)) {
    NaClLog(LOG_FATAL, "Manifest proxy ctor failed\n");
    goto cleanup_manifest_proxy;
  }

  /*
   * NaClSimpleServiceStartServiceThread requires the nap->mu lock.
   */
  if (!NaClSimpleServiceStartServiceThread((struct NaClSimpleService *)
                                           manifest_proxy)) {
    NaClLog(LOG_FATAL, "ManifestProxy start service failed\n");
    goto cleanup_manifest_proxy;
  }

  NaClXMutexLock(&nap->mu);
  (*NACL_VTBL(NaClNameService, nap->name_service)->
    CreateDescEntry)(nap->name_service,
                     "ManifestNameService", NACL_ABI_O_RDWR,
                     NaClDescRef(manifest_proxy->base.bound_and_cap[1]));
  NaClXMutexUnlock(&nap->mu);

  NaClXMutexLock(&self->mu);
  self->reverse_channel_initialization_state =
    NACL_REVERSE_CHANNEL_INITIALIZED;
  NaClXCondVarBroadcast(&self->cv);
  NaClXMutexUnlock(&self->mu);

 cleanup_manifest_proxy:
  NaClRefCountSafeUnref((struct NaClRefCount *) manifest_proxy);
 cleanup_reverse_quota_interface:
  NaClRefCountSafeUnref((struct NaClRefCount *) reverse_quota_interface);
 cleanup_reverse_host_interface:
  NaClRefCountSafeUnref((struct NaClRefCount *) reverse_host_interface);
 done:
  NaClLog(4, "Leaving NaClSecureReverseClientCallback\n");
}

static void NaClSecureServiceReverseSetupRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClSecureService        *nssp =
      (struct NaClSecureService *) rpc->channel->server_instance_data;
  struct NaClDesc                 *rev_addr = NULL;
  struct NaClSecureReverseClient  *rev;
  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4, "NaClSecureServiceReverseSetupRpc: reverse setup\n");

  NaClXMutexLock(&nssp->mu);
  if (NACL_REVERSE_CHANNEL_UNINITIALIZED !=
      nssp->reverse_channel_initialization_state) {
    NaClLog(LOG_ERROR, "NaClSecureServiceReverseSetupRpc:"
            " reverse channel initialization state not uninitialized\n");
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    goto cleanup;
  }
  nssp->reverse_channel_initialization_state =
      NACL_REVERSE_CHANNEL_INITIALIZATING;
  NaClXCondVarBroadcast(&nssp->cv);

  /* the reverse connection is still coming */
  rev = (struct NaClSecureReverseClient *) malloc(sizeof *rev);
  if (NULL == rev) {
    rpc->result = NACL_SRPC_RESULT_NO_MEMORY;
    goto cleanup;
  }
  NaClLog(4, "NaClSecureServiceReverseSetupRpc:"
          " invoking NaClSecureReverseClientCtor\n");
  if (!NaClSecureReverseClientCtor(rev,
                                   NaClSecureReverseClientCallback,
                                   (void *) nssp)) {
    free(rev);
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    goto cleanup;
  }

  if (!NaClSimpleRevClientStartServiceThread(&rev->base)) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    goto cleanup;
  }

  nssp->reverse_client = (struct NaClSecureReverseClient *) NaClRefCountRef(
      (struct NaClRefCount *) rev);

  out_args[0]->u.hval = NaClDescRef(rev->base.bound_and_cap[1]);
  rpc->result = NACL_SRPC_RESULT_OK;
 cleanup:
  NaClXMutexUnlock(&nssp->mu);
  NaClLog(4, "NaClSecureServiceReverseSetupRpc: done, "
          " rev_addr 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) rev_addr);
  (*done_cls->Run)(done_cls);
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

  /*
   * When reverse setup is being used, we have to block and wait for reverse
   * channel to become initialized before we can proceed with start module.
   */
  NaClXMutexLock(&nssp->mu);
  if (NACL_REVERSE_CHANNEL_UNINITIALIZED !=
      nssp->reverse_channel_initialization_state) {
    while (NACL_REVERSE_CHANNEL_INITIALIZED !=
           nssp->reverse_channel_initialization_state) {
      NaClXCondVarWait(&nssp->cv, &nssp->mu);
    }
  }
  NaClXMutexUnlock(&nssp->mu);

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

static void NaClSecureServiceLogRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  int   severity = in_args[0]->u.ival;
  char  *msg = in_args[1]->arrays.str;
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(5, "NaClSecureChannelLogRpc\n");
  NaClLog(severity, "%s\n", msg);
  NaClLog(5, "NaClSecureChannelLogRpc\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
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
  { NACL_SECURE_SERVICE_REVERSE_SETUP, NaClSecureServiceReverseSetupRpc, },
  { NACL_SECURE_SERVICE_START_MODULE, NaClSecureServiceStartModuleRpc, },
  { NACL_SECURE_SERVICE_LOG, NaClSecureServiceLogRpc, },
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

struct NaClSecureRevClientConnHandler {
  struct NaClSecureRevClientConnHandler  *next;

  /* used by NaClSimpleRevServiceClient's ClientCallback fn */
  void                                   (*handler)(
      void                                          *state,
      struct NaClThreadInterface                    *tif,
      struct NaClDesc                               *conn);
  void                                   *state;
};

static void NaClSecureReverseClientInternalCallback(
    void                        *state,
    struct NaClThreadInterface  *tif,
    struct NaClDesc             *conn) {
  struct NaClSecureReverseClient *self =
      (struct NaClSecureReverseClient *) state;
  struct NaClSecureRevClientConnHandler *hand_ptr;

  NaClLog(4, "Entered NaClSecureReverseClientInternalCallback\n");
  hand_ptr = (*NACL_VTBL(NaClSecureReverseClient, self)->RemoveHandler)(self);
  NaClLog(4, " got callback object %"NACL_PRIxPTR"\n", (uintptr_t) hand_ptr);
  NaClLog(4,
          " callback:0x%"NACL_PRIxPTR"(0x%"NACL_PRIxPTR",0x%"NACL_PRIxPTR")\n",
          (uintptr_t) hand_ptr->handler,
          (uintptr_t) hand_ptr->state,
          (uintptr_t) conn);
  (*hand_ptr->handler)(hand_ptr->state, tif, conn);
  NaClLog(4, "NaClSecureReverseClientInternalCallback: freeing memory\n");
  free(hand_ptr);
  NaClLog(4, "Leaving NaClSecureReverseClientInternalCallback\n");
}

/*
 * Require an initial connection handler in the Ctor, so that it's
 * obvious that a reverse client needs to accept an IMC connection
 * from the server to get things bootstrapped.
 */
int NaClSecureReverseClientCtor(
    struct NaClSecureReverseClient  *self,
    void                            (*client_callback)(
        void *, struct NaClThreadInterface*, struct NaClDesc *),
    void                            *state) {
  NaClLog(4,
          "Entered NaClSecureReverseClientCtor, self 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  if (!NaClSimpleRevClientCtor(&self->base,
                               NaClSecureReverseClientInternalCallback,
                               (void *) self,
                               NaClThreadInterfaceThreadFactory,
                               (void *) NULL)) {
    goto failure_simple_ctor;
  }
  NaClLog(4, "NaClSecureReverseClientCtor: Mutex\n");
  if (!NaClMutexCtor(&self->mu)) {
    goto failure_mutex_ctor;
  }
  self->queue_head = (struct NaClSecureRevClientConnHandler *) NULL;
  self->queue_insert = &self->queue_head;

  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSecureReverseClientVtbl;

  NaClLog(4, "NaClSecureReverseClientCtor: InsertHandler\n");
  if (!(*NACL_VTBL(NaClSecureReverseClient, self)->
        InsertHandler)(self, client_callback, state)) {
    goto failure_handler_insert;
  }

  NaClLog(4, "Leaving NaClSecureReverseClientCtor\n");
  return 1;

 failure_handler_insert:
  NaClLog(4, "NaClSecureReverseClientCtor: InsertHandler failed\n");
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSimpleRevClientVtbl;

  self->queue_insert = (struct NaClSecureRevClientConnHandler **) NULL;
  NaClMutexDtor(&self->mu);

 failure_mutex_ctor:
  NaClLog(4, "NaClSecureReverseClientCtor: Mutex failed\n");
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
 failure_simple_ctor:
  NaClLog(4, "Leaving NaClSecureReverseClientCtor\n");
  return 0;
}

void NaClSecureReverseClientDtor(struct NaClRefCount *vself) {
  struct NaClSecureReverseClient *self =
      (struct NaClSecureReverseClient *) vself;

  struct NaClSecureRevClientConnHandler  *entry;
  struct NaClSecureRevClientConnHandler  *next;

  for (entry = self->queue_head; NULL != entry; entry = next) {
    next = entry->next;
    free(entry);
  }
  NaClMutexDtor(&self->mu);

  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl const *)
      &kNaClSimpleRevClientVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

/*
 * Caller must set up handler before issuing connection request RPC on
 * nap->reverse_channel, since otherwise the connection handler queue
 * may be empty and the connect code would abort.  Because the connect
 * doesn't wait for a handler, we don't need a condvar.
 *
 * We do not need to serialize on the handlers, since the
 * RPC-server/IMC-client implementation should not distinguish one
 * connection from another: it is okay for two handlers to be
 * inserted, and two connection request RPCs to be preformed
 * (sequentially, since they are over a single channel), and have the
 * server side spawn threads that asynchronously connect twice, in the
 * "incorrect" order, etc.
 */
int NaClSecureReverseClientInsertHandler(
    struct NaClSecureReverseClient  *self,
    void                            (*handler)(
        void                        *handler_state,
        struct NaClThreadInterface  *thread_if,
        struct NaClDesc             *new_conn),
    void                            *state) {
  struct NaClSecureRevClientConnHandler *entry;
  int                                   retval = 0; /* fail */

  NaClLog(4,
          ("NaClSecureReverseClientInsertHandler: "
           "handler 0x%"NACL_PRIxPTR", state 0x%"NACL_PRIxPTR"\n"),
          (uintptr_t) handler, (uintptr_t) state);

  NaClXMutexLock(&self->mu);

  entry = (struct NaClSecureRevClientConnHandler *) malloc(sizeof *entry);
  if (NULL == entry) {
    goto cleanup;
  }
  entry->handler = handler;
  entry->state = state;
  entry->next = (struct NaClSecureRevClientConnHandler *) NULL;
  *self->queue_insert = entry;
  self->queue_insert = &entry->next;
  retval = 1;

 cleanup:
  NaClXMutexUnlock(&self->mu);
  return retval;
}

struct NaClSecureRevClientConnHandler *NaClSecureReverseClientRemoveHandler(
    struct NaClSecureReverseClient *self) {
  struct NaClSecureRevClientConnHandler *head;

  NaClLog(4, "Entered NaClSecureReverseClientRemoveHandler, acquiring lock\n");
  NaClXMutexLock(&self->mu);
  NaClLog(4, "NaClSecureReverseClientRemoveHandler, got lock\n");
  head = self->queue_head;
  if (NULL == head) {
    NaClLog(LOG_FATAL,
            "NaClSecureReverseClientRemoveHandler:  empty handler queue\n");
  }
  if (NULL == (self->queue_head = head->next)) {
    NaClLog(4, "NaClSecureReverseClientRemoveHandler, last elt patch up\n");
    self->queue_insert = &self->queue_head;
  }
  NaClLog(4, "NaClSecureReverseClientRemoveHandler, unlocking\n");
  NaClXMutexUnlock(&self->mu);

  head->next = NULL;
  NaClLog(4,
          ("Leaving NaClSecureReverseClientRemoveHandler:"
           " returning %"NACL_PRIxPTR"\n"),
          (uintptr_t) head);
  return head;
}

struct NaClSecureReverseClientVtbl const kNaClSecureReverseClientVtbl = {
  {
    {
      NaClSecureReverseClientDtor,
    },
  },
  NaClSecureReverseClientInsertHandler,
  NaClSecureReverseClientRemoveHandler,
};
