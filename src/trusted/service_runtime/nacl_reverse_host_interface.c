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

int NaClReverseHostInterfaceLog(
    struct NaClRuntimeHostInterface *vself,
    char const                      *message) {
  struct NaClReverseHostInterface *self =
      (struct NaClReverseHostInterface *) vself;
  NaClSrpcError           rpc_result;
  int                     status = 0;

  NaClLog(3,
          "NaClReverseHostInterfaceLog(0x%08"NACL_PRIxPTR", %s)\n",
          (uintptr_t) self, message);

  NaClXMutexLock(&self->server->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      self->server->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&self->server->reverse_channel,
                                           NACL_REVERSE_CONTROL_LOG,
                                           message);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              "NaClReverseHostInterfaceLog: RPC failed, result %d\n",
              rpc_result);
    }
  } else {
    NaClLog(4, "NaClReverseHostInterfaceLog: no reverse channel"
            ", no plugin to talk to.\n");
    status = -NACL_ABI_ENODEV;
  }
  NaClXMutexUnlock(&self->server->mu);
  return status;
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

ssize_t NaClReverseHostInterfacePostMessage(
    struct NaClRuntimeHostInterface *vself,
    char const                      *message,
    size_t                          message_bytes) {
  struct NaClReverseHostInterface *self =
      (struct NaClReverseHostInterface *) vself;
  NaClSrpcError           rpc_result;
  ssize_t                 num_written;

  NaClLog(3,
          ("NaClReverseHostInterfacePostMessage(0x%08"NACL_PRIxPTR", %s"
           ", %08"NACL_PRIdS")\n"),
          (uintptr_t) self, message, message_bytes);

  NaClXMutexLock(&self->server->mu);
  if (message_bytes > NACL_ABI_SIZE_T_MAX) {
    message_bytes = NACL_ABI_SIZE_T_MAX;
  }
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      self->server->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(&self->server->reverse_channel,
                                           NACL_REVERSE_CONTROL_POST_MESSAGE,
                                           message_bytes,
                                           message,
                                           &num_written);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              "NaClReverseHostInterfacePostMessage: RPC failed, result %d\n",
              rpc_result);
    }
  } else {
    NaClLog(4, "NaClReverseHostInterfacePostMessage: no reverse channel"
            ", no plugin to talk to.\n");
    num_written = -NACL_ABI_ENODEV;
  }
  NaClXMutexUnlock(&self->server->mu);
  return num_written;
}

int NaClReverseHostInterfaceCreateProcess(
    struct NaClRuntimeHostInterface *vself,
    struct NaClDesc                 **out_sock_addr,
    struct NaClDesc                 **out_app_addr) {
  struct NaClReverseHostInterface *self =
      (struct NaClReverseHostInterface *) vself;
  NaClSrpcError   rpc_result;
  int             pid = 0;

  NaClLog(3,
          ("NaClReverseHostInterfaceCreateProcess(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) self,
          (uintptr_t) out_sock_addr,
          (uintptr_t) out_app_addr);

  NaClXMutexLock(&self->server->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED ==
      self->server->reverse_channel_initialization_state) {
    rpc_result = NaClSrpcInvokeBySignature(
        &self->server->reverse_channel,
        NACL_REVERSE_CONTROL_CREATE_PROCESS_INTERLOCKED,
        out_sock_addr,
        out_app_addr,
        &pid);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_FATAL,
              "NaClReverseHostInterfaceCreateProcess: RPC failed, result %d\n",
              rpc_result);
    }
  } else {
    NaClLog(4, "NaClReverseHostInterfaceCreateProcess: no reverse channel"
            ", no plugin to talk to.\n");
    pid = -NACL_ABI_ENODEV;
  }
  NaClXMutexUnlock(&self->server->mu);
  return pid;
}

struct NaClRuntimeHostInterfaceVtbl const kNaClReverseHostInterfaceVtbl = {
  {
    NaClReverseHostInterfaceDtor,
  },
  NaClReverseHostInterfaceLog,
  NaClReverseHostInterfaceStartupInitializationComplete,
  NaClReverseHostInterfaceReportExitStatus,
  NaClReverseHostInterfacePostMessage,
  NaClReverseHostInterfaceCreateProcess,
};
