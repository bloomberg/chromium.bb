/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_reverse_quota_interface.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_quota.h"
#include "native_client/src/trusted/desc/nacl_desc_quota_interface.h"

#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


/*
 * A descriptor quota interface that works over the reverse channel to Chrome.
 */

struct NaClDescQuotaInterfaceVtbl const kNaClReverseQuotaInterfaceVtbl;

int NaClReverseQuotaInterfaceCtor(
    struct NaClReverseQuotaInterface *self,
    struct NaClSecureService         *server) {
  NaClLog(4, "NaClReverseQuotaInterfaceCtor:"
          " self 0x%"NACL_PRIxPTR", server 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self, (uintptr_t) server);
  if (!NaClDescQuotaInterfaceCtor(&self->base)) {
    return 0;
  }
  self->server = (struct NaClSecureService *)
      NaClRefCountRef((struct NaClRefCount *) server);
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl const *) &kNaClReverseQuotaInterfaceVtbl;
  return 1;
}

static void NaClReverseQuotaInterfaceDtor(struct NaClRefCount *vself) {
  struct NaClReverseQuotaInterface *self =
      (struct NaClReverseQuotaInterface *) vself;
  NaClLog(4, "NaClReverseQuotaInterfaceDtor\n");

  NaClRefCountUnref((struct NaClRefCount *) self->server);

  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClDescQuotaInterfaceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

static int64_t NaClReverseQuotaInterfaceWriteRequest(
    struct NaClDescQuotaInterface *vself,
    uint8_t const                 *file_id,
    int64_t                       offset,
    int64_t                       length) {
  struct NaClReverseQuotaInterface  *self =
      (struct NaClReverseQuotaInterface *) vself;
  NaClSrpcError                     rpc_result;
  int64_t                           allowed;
  int64_t                           rv;

  NaClLog(4, "Entered NaClReverseQuotaWriteRequest\n");
  NaClXMutexLock(&self->server->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED !=
      self->server->reverse_channel_initialization_state) {
    NaClLog(LOG_FATAL,
            "NaClReverseQuotaWriteRequest: Reverse channel not initialized\n");
  }
  rpc_result = NaClSrpcInvokeBySignature(&self->server->reverse_channel,
                                         NACL_REVERSE_REQUEST_QUOTA_FOR_WRITE,
                                         16,
                                         file_id,
                                         offset,
                                         length,
                                         &allowed);
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    rv = 0;
  } else {
    rv = allowed;
  }
  NaClXMutexUnlock(&self->server->mu);
  NaClLog(4, "Leaving NaClReverseQuotaWriteRequest\n");
  return rv;
}

static int64_t NaClReverseQuotaInterfaceFtruncateRequest(
    struct NaClDescQuotaInterface *self,
    uint8_t const                 *file_id,
    int64_t                       length) {
  UNREFERENCED_PARAMETER(self);
  UNREFERENCED_PARAMETER(file_id);

  NaClLog(LOG_FATAL, "FtruncateRequest invoked!?!\n");
  return length;
}

struct NaClDescQuotaInterfaceVtbl const kNaClReverseQuotaInterfaceVtbl = {
  {
    NaClReverseQuotaInterfaceDtor,
  },
  NaClReverseQuotaInterfaceWriteRequest,
  NaClReverseQuotaInterfaceFtruncateRequest,
};
