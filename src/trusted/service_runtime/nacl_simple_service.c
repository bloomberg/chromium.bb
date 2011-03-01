/*
 * Copyright 2011 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_simple_service.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"

#include "native_client/src/trusted/nacl_base/nacl_refcount.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
/* NACL_KERN_STACK_SIZE */
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

int NaClSimpleServiceConnectionCtor(struct NaClSimpleServiceConnection  *self,
                                    struct NaClSimpleService            *server,
                                    struct NaClDesc                     *conn) {
  if (!NaClRefCountCtor((struct NaClRefCount *) self)) {
    return 0;
  }
  self->server = (struct NaClSimpleService *) NaClRefCountRef(
      (struct NaClRefCount *) server);
  self->connected_socket = (struct NaClDesc *) NaClRefCountRef(
      (struct NaClRefCount *) conn);
  self->thread_spawned = 0;
  self->base.vtbl = (struct NaClRefCountVtbl const *)
      &kNaClSimpleServiceConnectionVtbl;
  return 1;
}

void NaClSimpleServiceConnectionDtor(struct NaClRefCount *vself) {
  struct NaClSimpleServiceConnection *self =
      (struct NaClSimpleServiceConnection *) vself;

  NaClRefCountUnref((struct NaClRefCount *) self->server);
  NaClRefCountUnref((struct NaClRefCount *) self->connected_socket);
  if (self->thread_spawned) {
    NaClThreadDtor(&self->thr);
  }
  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl const *)
      &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClSimpleServiceConnectionServerLoop(
    struct NaClSimpleServiceConnection *self) {
  int retval;

  NaClLog(4,
          "NaClSimpleServiceConnectionServerLoop: handling RPC requests from"
          " client at socket %"NACL_PRIxPTR"\n",
          (uintptr_t) self->connected_socket);
  retval = NaClSrpcServerLoop(self->connected_socket,
                              self->server->handlers,
                              self->server->instance_data);
  NaClLog(4,
          "NaClSimpleServiceConnectionServerLoop: NaClSrpcServerLoop exited,"
          " value %d\n", retval);
  return retval;
}

struct NaClSimpleServiceConnectionVtbl
const kNaClSimpleServiceConnectionVtbl = {
  {
    NaClSimpleServiceConnectionDtor,
  },
  NaClSimpleServiceConnectionServerLoop,
};


/* default rpc_handler */
void NaClSimpleServiceRpcHandler(
    struct NaClSimpleService           *self,
    struct NaClSimpleServiceConnection *conn) {
  UNREFERENCED_PARAMETER(self);

  (*NACL_VTBL(NaClSimpleServiceConnection, conn)->ServerLoop)(
      conn);
}

static void WINAPI RpcHandlerBase(void *thread_state) {
  struct NaClSimpleServiceConnection  *conn =
      (struct NaClSimpleServiceConnection *) thread_state;

  NaClLog(4, "Entered RpcHandlerBase, invoking RpcHandler virtual fn\n");
  (*NACL_VTBL(NaClSimpleService, conn->server)->RpcHandler)(conn->server,
                                                            conn);
  NaClRefCountUnref((struct NaClRefCount *) conn);
  NaClLog(4, "Leaving RpcHandlerBase\n");
}

int NaClSimpleServiceCtorIntern(
    struct NaClSimpleService          *self,
    struct NaClSrpcHandlerDesc const  *srpc_handlers,
    void                              *instance_data) {
  NaClLog(4, "Entered NaClSimpleServiceCtorIntern\n");
  if (!NaClRefCountCtor((struct NaClRefCount *) self)) {
    NaClLog(4, "NaClSimpleServiceCtorIntern: NaClRefCountCtor failed\n");
    return 0;
  }
  self->handlers = srpc_handlers;
  self->instance_data = instance_data;
  self->acceptor_spawned = 0;
  self->base.vtbl = (struct NaClRefCountVtbl const *) &kNaClSimpleServiceVtbl;
  NaClLog(4, "Leaving NaClSimpleServiceCtorIntern\n");
  return 1;
}

int NaClSimpleServiceCtor(
    struct NaClSimpleService          *self,
    struct NaClSrpcHandlerDesc const  *srpc_handlers,
    void                              *instance_data) {
  if (0 != NaClCommonDescMakeBoundSock(self->bound_and_cap)) {
    return 0;
  }
  if (!NaClSimpleServiceCtorIntern(self, srpc_handlers, instance_data)) {
    NaClDescUnref(self->bound_and_cap[0]);
    NaClDescUnref(self->bound_and_cap[1]);
    return 0;
  }
  return 1;
}

int NaClSimpleServiceWithSocketCtor(
    struct NaClSimpleService          *self,
    struct NaClSrpcHandlerDesc const  *srpc_handlers,
    void                              *instance_data,
    struct NaClDesc                   *service_port,
    struct NaClDesc                   *sock_addr) {
  if (!NaClSimpleServiceCtorIntern(self, srpc_handlers, instance_data)) {
    return 0;
  }
  self->bound_and_cap[0] = NaClDescRef(service_port);
  self->bound_and_cap[1] = NaClDescRef(sock_addr);

  return 1;
}

void NaClSimpleServiceDtor(struct NaClRefCount *vself) {
  struct NaClSimpleService *self = (struct NaClSimpleService *) vself;

  NaClRefCountSafeUnref((struct NaClRefCount *) self->bound_and_cap[0]);
  NaClRefCountSafeUnref((struct NaClRefCount *) self->bound_and_cap[1]);

  if (self->acceptor_spawned) {
    NaClThreadDtor(&self->acceptor);
  }

  self->base.vtbl = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClSimpleServiceConnectionFactory(
    struct NaClSimpleService            *self,
    struct NaClDesc                     *conn,
    struct NaClSimpleServiceConnection  **out) {
  struct NaClSimpleServiceConnection  *server_conn;
  int                                 status;

  NaClLog(4, "Entered NaClSimpleServiceConnectionFactory\n");
  server_conn = malloc(sizeof *server_conn);
  if (NULL == server_conn) {
    status = -NACL_ABI_ENOMEM;
    goto abort;
  }
  NaClLog(4,
          "NaClSimpleServiceConnectionFactory: self 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  NaClLog(4,
          "NaClSimpleServiceConnectionFactory: conn 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) conn);
  NaClLog(4,
          "NaClSimpleServiceConnectionFactory: out 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) out);
  if (!NaClSimpleServiceConnectionCtor(server_conn, self, conn)) {
    free(server_conn);
    status = -NACL_ABI_EIO;
    goto abort;
  }
  NaClLog(4,
          "NaClSimpleServiceConnectionFactory: constructed 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) server_conn);
  *out = server_conn;
  status = 0;
 abort:
  NaClLog(4, "Leaving NaClSimpleServiceConnectionFactory: status %d\n", status);
  return status;
}

int NaClSimpleServiceAcceptConnection(
    struct NaClSimpleService            *self,
    struct NaClSimpleServiceConnection  **out) {
  int                                 status = -NACL_ABI_EINVAL;
  struct NaClSimpleServiceConnection  *conn = NULL;
  struct NaClDesc                     *connected_desc = NULL;

  NaClLog(4, "Entered NaClSimpleServiceAcceptConnection\n");
  conn = malloc(sizeof *conn);
  if (NULL == conn) {
    return -NACL_ABI_ENOMEM;
  }
  /* NB: conn is allocated but not constructed */
  status = (*NACL_VTBL(NaClDesc, self->bound_and_cap[0])->
            AcceptConn)(self->bound_and_cap[0], &connected_desc);
  if (0 != status) {
    NaClLog(4, "Accept failed\n");
    free(conn);
    conn = NULL;
    goto cleanup;
  }

  NaClLog(4,
          "connected_desc is 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) connected_desc);

  status = (*NACL_VTBL(NaClSimpleService, self)->ConnectionFactory)(
      self,
      connected_desc,
      &conn);
  if (0 != status) {
    NaClLog(4, "ConnectionFactory failed\n");
    goto cleanup;
  }
  /* all okay! */
  NaClLog(4, "conn is 0x%"NACL_PRIxPTR"\n", (uintptr_t) conn);
  NaClLog(4, "out  is 0x%"NACL_PRIxPTR"\n", (uintptr_t) out);
  *out = conn;
  status = 0;
cleanup:
  NaClRefCountSafeUnref((struct NaClRefCount *) connected_desc);
  NaClLog(4, "Leaving NaClSimpleServiceAcceptConnection, status %d\n", status);
  return status;
}

int NaClSimpleServiceAcceptAndSpawnHandler(
    struct NaClSimpleService *self) {
  struct NaClSimpleServiceConnection  *conn = NULL;
  int                                 status;

  NaClLog(4, "Entered NaClSimpleServiceAcceptAndSpawnHandler\n");
  NaClLog(4,
          ("NaClSimpleServiceAcceptAndSpawnHandler:"
           " &conn is 0x%"NACL_PRIxPTR"\n"),
          (uintptr_t) &conn);
  status = (*NACL_VTBL(NaClSimpleService, self)->AcceptConnection)(
      self,
      &conn);
  NaClLog(4, "AcceptConnection vfn returned %d\n", status);
  if (0 != status) {
    goto abort;
  }
  NaClLog(4, "conn is 0x%"NACL_PRIxPTR"\n", (uintptr_t) conn);
  NaClLog(4, "NaClSimpleServiceAcceptAndSpawnHandler: spawning thread\n");
  conn->thread_spawned = 1;
  /* ownership of |conn| reference is passed to the thread */
  NaClThreadCtor(&conn->thr,
                 RpcHandlerBase,
                 conn,
                 NACL_KERN_STACK_SIZE);
  status = 0;
abort:
  NaClLog(4,
          "Leaving NaClSimpleServiceAcceptAndSpawnHandler, status %d\n",
          status);
  return 0;
}


struct NaClSimpleServiceVtbl const kNaClSimpleServiceVtbl = {
  {
    NaClSimpleServiceDtor,
  },
  NaClSimpleServiceConnectionFactory,
  NaClSimpleServiceAcceptConnection,
  NaClSimpleServiceAcceptAndSpawnHandler,
  NaClSimpleServiceRpcHandler,
};




static void WINAPI AcceptThread(void *thread_state) {
  struct NaClSimpleService *server =
      (struct NaClSimpleService *) thread_state;

  NaClLog(4, "Entered AcceptThread\n");
  while (0 == (*NACL_VTBL(NaClSimpleService, server)->
               AcceptAndSpawnHandler)(server)) {
    NaClLog(4, "AcceptThread: accepted, looping to next thread\n");
    continue;
  }
  NaClLog(4, "Leaving AcceptThread\n");
}

int NaClSimpleServiceStartServiceThread(struct NaClSimpleService *server) {

  if (NaClThreadCtor(&server->acceptor,
                     AcceptThread,
                     server,
                     NACL_KERN_STACK_SIZE)) {
    server->acceptor_spawned = 1;
    return 1;
  }
  return 0;
}
