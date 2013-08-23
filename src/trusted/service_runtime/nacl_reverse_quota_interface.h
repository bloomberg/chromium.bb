/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_REVERSE_QUOTA_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_REVERSE_QUOTA_INTERFACE_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_quota.h"
#include "native_client/src/trusted/desc/nacl_desc_quota_interface.h"

EXTERN_C_BEGIN

struct NaClSecureService;

/*
 * A descriptor quota interface that works over the reverse channel to Chrome.
 */

struct NaClReverseQuotaInterface {
  struct NaClDescQuotaInterface base NACL_IS_REFCOUNT_SUBCLASS;

  struct NaClSecureService      *server;
};

int NaClReverseQuotaInterfaceCtor(
    struct NaClReverseQuotaInterface    *self,
    struct NaClSecureService            *server);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_REVERSE_QUOTA_INTERFACE_H_ */
