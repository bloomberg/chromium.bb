/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_secure_service.h"

#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/fault_injection/fault_injection.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


int NaClSecureServiceCtor(struct NaClSecureService          *self,
                          struct NaClSrpcHandlerDesc const  *srpc_handlers,
                          struct NaClApp                    *nap,
                          struct NaClDesc                   *service_port,
                          struct NaClDesc                   *sock_addr) {
  NaClLog(4,
          "Entered NaClSecureServiceCtor: self 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  if (NACL_FI_ERROR_COND(
          "NaClSecureServiceCtor__NaClSimpleServiceWithSocketCtor",
          !NaClSimpleServiceWithSocketCtor(
              &self->base,
              srpc_handlers,
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
  self->nap = nap;
  self->conn_count = 0;
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSecureServiceVtbl;
  return 1;

 failure_mutex_ctor:
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
 done:
  return 0;
}

void NaClSecureServiceDtor(struct NaClRefCount *vself) {
  struct NaClSecureService *self = (struct NaClSecureService *) vself;

  if (0 != self->conn_count) {
    NaClLog(LOG_FATAL,
            "SecureService dtor when connection count is nonzero\n");
  }
  NaClMutexDtor(&self->mu);

  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl const *)
      &kNaClSimpleServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClSecureServiceConnectionFactory(
    struct NaClSimpleService            *vself,
    struct NaClDesc                     *conn,
    struct NaClSimpleServiceConnection  **out) {
  struct NaClSecureService *self =
      (struct NaClSecureService *) vself;

  /* our instance_data is not connection specific */
  return NaClSimpleServiceConnectionFactoryWithInstanceData(
      vself, conn, (void *) self->nap, out);
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
  NaClLog(5, "NaClSecureServiceThreadCountDecr\n");
  NaClXMutexLock(&self->mu);
  if (0 == self->conn_count) {
    NaClLog(LOG_FATAL,
            "NaClSecureServiceThreadCountDecr: "
            "decrementing thread count when count is zero\n");
  }
  if (0 == --self->conn_count) {
    NaClLog(4, "NaClSecureServiceThread: all channels closed, exiting.\n");
    NaClExit(0);
  }
  NaClXMutexUnlock(&self->mu);
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
    void                            *state,
    struct NaClApp                  *nap) {
  NaClLog(4,
          ("Entered NaClSecureReverseClientCtor, self 0x%"NACL_PRIxPTR","
           " nap 0x%"NACL_PRIxPTR"\n"),
          (uintptr_t) self,
          (uintptr_t) nap);
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
  self->nap = nap;
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

  self->nap = NULL;
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
