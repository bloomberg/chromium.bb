/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SECURE_SERVICE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SECURE_SERVICE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/simple_service/nacl_simple_service.h"
#include "native_client/src/trusted/simple_service/nacl_simple_rservice.h"

EXTERN_C_BEGIN

struct NaClApp;

/*
 * Secure channel.
 */

struct NaClSecureService {
  struct NaClSimpleService        base NACL_IS_REFCOUNT_SUBCLASS;
  struct NaClApp                  *nap;

  /*
   * |mu| and |cv| protects the reverse channel initialization state
   * and connection count access.
   */
  struct NaClCondVar              cv;
  struct NaClMutex                mu;

  enum NaClReverseChannelInitializationState {
    NACL_REVERSE_CHANNEL_UNINITIALIZED = 0,
    NACL_REVERSE_CHANNEL_INITIALIZATING,
    NACL_REVERSE_CHANNEL_INITIALIZED
  }                               reverse_channel_initialization_state;
  struct NaClSrpcChannel          reverse_channel;
  struct NaClSecureReverseClient  *reverse_client;

  uint32_t                        conn_count;
};

int NaClSecureServiceCtor(
    struct NaClSecureService          *self,
    struct NaClApp                    *nap,
    struct NaClDesc                   *service_port,
    struct NaClDesc                   *sock_addr);

void NaClSecureServiceDtor(struct NaClRefCount *vself);

extern struct NaClSimpleServiceVtbl const kNaClSecureServiceVtbl;

void NaClSecureCommandChannel(struct NaClApp *nap);

struct NaClSecureRevClientConnHandler;  /* fwd */

struct NaClSecureReverseClient {
  struct NaClSimpleRevClient              base;

  struct NaClMutex                        mu;
  /*
   * |mu| protects the service entries hanging off of |queue_head|.
   */
  struct NaClSecureRevClientConnHandler   *queue_head;
  struct NaClSecureRevClientConnHandler   **queue_insert;
};

struct NaClSecureReverseClientVtbl {
  struct NaClSimpleRevClientVtbl  vbase;

  int                             (*InsertHandler)(
      struct NaClSecureReverseClient *self,
      void                           (*handler)(
          void                                  *handler_state,
          struct NaClThreadInterface            *thread_id,
          struct NaClDesc                       *new_conn),
      void                           *state);

  struct NaClSecureRevClientConnHandler   *(*RemoveHandler)(
     struct NaClSecureReverseClient *self);
};

int NaClSecureReverseClientCtor(
    struct NaClSecureReverseClient *self,
    void                            (*client_callback)(
        void *, struct NaClThreadInterface *, struct NaClDesc *),
    void                            *state);

void NaClSecureReverseClientDtor(struct NaClRefCount *vself);

int NaClSecureReverseClientInsertHandler(
    struct NaClSecureReverseClient  *self,
    void                            (*handler)(
        void                                   *handler_state,
        struct NaClThreadInterface             *thread_if,
        struct NaClDesc                        *new_conn),
    void                            *state) NACL_WUR;

struct NaClSecureRevClientConnHandler *NaClSecureReverseClientRemoveHandler(
    struct NaClSecureReverseClient *self);

extern struct NaClSecureReverseClientVtbl const kNaClSecureReverseClientVtbl;

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SECURE_SERVICE_H_ */
