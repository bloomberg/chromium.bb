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

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SECURE_SERVICE_H_ */
