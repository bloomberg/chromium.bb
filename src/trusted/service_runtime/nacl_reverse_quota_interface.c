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

static void ReverseQuotaDtor(struct NaClRefCount *nrcp) {
  NaClLog(4, "NaClReverseQuotaInterfaceDtor\n");
  nrcp->vtbl = (struct NaClRefCountVtbl *)(&kNaClDescQuotaInterfaceVtbl);
  (*nrcp->vtbl->Dtor)(nrcp);
}

static int64_t ReverseQuotaWriteRequest(
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
  NaClXMutexLock(&self->nap->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED !=
      self->nap->reverse_channel_initialization_state) {
    NaClLog(LOG_FATAL,
            "NaClReverseQuotaWriteRequest: Reverse channel not initialized\n");
  }
  rpc_result = NaClSrpcInvokeBySignature(&self->nap->reverse_channel,
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
  NaClXMutexUnlock(&self->nap->mu);
  NaClLog(4, "Leaving NaClReverseQuotaWriteRequest\n");
  return rv;
}

static int64_t ReverseQuotaFtruncateRequest(
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
    ReverseQuotaDtor
  },
  ReverseQuotaWriteRequest,
  ReverseQuotaFtruncateRequest
};

int NaClReverseQuotaInterfaceCtor(struct NaClReverseQuotaInterface *self,
                                  struct NaClApp *nap) {
  struct NaClRefCount *nrcp = (struct NaClRefCount *) self;
  NaClLog(4, "NaClReverseQuotaInterfaceCtor\n");
  if (!NaClDescQuotaInterfaceCtor(&(self->base))) {
    return 0;
  }
  nrcp->vtbl = (struct NaClRefCountVtbl *) (&kNaClReverseQuotaInterfaceVtbl);
  self->nap = nap;
  return 1;
}
