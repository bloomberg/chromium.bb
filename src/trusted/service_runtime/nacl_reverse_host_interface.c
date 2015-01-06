/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_reverse_host_interface.h"

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/nacl_runtime_host_interface.h"
#include "native_client/src/trusted/service_runtime/nacl_secure_service.h"


struct NaClRuntimeHostInterfaceVtbl const kNaClReverseHostInterfaceVtbl;

int NaClReverseHostInterfaceCtor(
    struct NaClReverseHostInterface *self,
    struct NaClSecureService        *server) {
  NaClLog(4, "NaClReverseHostInterfaceCtor:"
          "self 0x%"NACL_PRIxPTR", server 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self, (uintptr_t) server);

  if (!NaClRuntimeHostInterfaceCtor_protected(&self->base)) {
    NaClLog(3, "NaClReverseHostInterfaceCtor: "
            "NaClRuntimeHostInterfaceCtor base class ctor failed\n");
    return 0;
  }
  self->server = (struct NaClSecureService *)
      NaClRefCountRef((struct NaClRefCount *) server);
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl const *) &kNaClReverseHostInterfaceVtbl;
  return 1;
}

void NaClReverseHostInterfaceDtor(struct NaClRefCount *vself) {
  struct NaClReverseHostInterface *self =
      (struct NaClReverseHostInterface *) vself;

  NaClRefCountUnref((struct NaClRefCount *) self->server);

  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClReverseHostInterfaceStartupInitializationComplete(
    struct NaClRuntimeHostInterface *vself) {
  struct NaClReverseHostInterface *self =
      (struct NaClReverseHostInterface *) vself;
  NaClSrpcError           rpc_result;
  int                     status = 0;

  NaClLog(3,
          ("NaClReverseHostInterfaceStartupInitializationComplete(0x%08"
           NACL_PRIxPTR")\n"),
          (uintptr_t) self);

  NaClXMutexLock(&self->server->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      self->server->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&self->server->reverse_channel,
                                           NACL_REVERSE_CONTROL_INIT_DONE);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              "NaClReverseHostInterfaceStartupInitializationComplete:"
              " RPC failed, result %d\n",
              rpc_result);
    }
  } else {
    NaClLog(4, "NaClReverseHostInterfaceStartupInitializationComplete:"
            " no reverse channel, no plugin to talk to.\n");
    status = -NACL_ABI_ENODEV;
  }
  NaClXMutexUnlock(&self->server->mu);
  return status;
}

int NaClReverseHostInterfaceReportExitStatus(
    struct NaClRuntimeHostInterface *vself,
    int                             exit_status) {
  struct NaClReverseHostInterface *self =
      (struct NaClReverseHostInterface *) vself;
  NaClSrpcError           rpc_result;
  int                     status = 0;

  NaClLog(3,
          "NaClReverseHostInterfaceReportExitStatus:"
          " self 0x%08"NACL_PRIxPTR", exit_status 0x%x)\n",
          (uintptr_t) self, exit_status);

  NaClXMutexLock(&self->server->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      self->server->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&self->server->reverse_channel,
                                           NACL_REVERSE_CONTROL_REPORT_STATUS,
                                           exit_status);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL, "NaClReverseHostInterfaceReportExitStatus:"
              " RPC failed, result %d\n",
              rpc_result);
    }
  } else {
    NaClLog(4, "NaClReverseHostInterfaceReportExitStatus: no reverse channel"
            ", no plugin to talk to.\n");
    status = -NACL_ABI_ENODEV;
  }
  NaClXMutexUnlock(&self->server->mu);
  return status;
}

struct NaClRuntimeHostInterfaceVtbl const kNaClReverseHostInterfaceVtbl = {
  {
    NaClReverseHostInterfaceDtor,
  },
  NaClReverseHostInterfaceStartupInitializationComplete,
  NaClReverseHostInterfaceReportExitStatus,
};
