/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#define NACL_LOG_MODULE_NAME "reverse_service_c"

#include "native_client/src/trusted/reverse_service/reverse_service_c.h"

#include <limits.h>
#include <string.h>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/portability_io.h"

#include "native_client/src/public/nacl_file_info.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

struct NaClSrpcHandlerDesc const kNaClReverseServiceHandlers[]; /* fwd */

int NaClReverseThreadIfFactoryFn(
    void                        *factory_data,
    NaClThreadIfStartFunction   fn_ptr,
    void                        *thread_data,
    size_t                      thread_stack_size,
    struct NaClThreadInterface  **out_new_thread); /* fwd */

int NaClReverseServiceCtor(struct NaClReverseService    *self,
                           struct NaClReverseInterface  *iface,
                           struct NaClDesc              *conn_cap) {
  int retval = 0; /* fail */

  CHECK(iface != NULL);

  NaClLog(4, "Entered NaClReverseServiceCtor\n");
  if (!NaClSimpleRevServiceCtor(&self->base,
                                conn_cap,
                                kNaClReverseServiceHandlers,
                                NaClReverseThreadIfFactoryFn,
                                (void *) self)) {
    NaClLog(4, "NaClReverseServiceCtor: NaClSimpleRevServiceCtor failed\n");
    goto done;
  }
  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl *)
      &kNaClReverseServiceVtbl;
  if (!NaClMutexCtor(&self->mu)) {
    NaClLog(4, "NaClMutexCtor failed\n");
    goto mutex_ctor_fail;
  }
  if (!NaClCondVarCtor(&self->cv)) {
    NaClLog(4, "NaClCondVar failed\n");
    goto condvar_ctor_fail;
  }
  /* success return path */
  self->iface = (struct NaClReverseInterface *) NaClRefCountRef(
      (struct NaClRefCount *) iface);
  self->thread_count = 0;

  retval = 1;
  goto done;

  /* cleanup unwind */
 condvar_ctor_fail:
  NaClMutexDtor(&self->mu);
 mutex_ctor_fail:
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
 done:
  return retval;
}

void NaClReverseServiceDtor(struct NaClRefCount *vself) {
  struct NaClReverseService *self = (struct NaClReverseService *) vself;

  if (0 != self->thread_count) {
    NaClLog(LOG_FATAL,
            "ReverseService dtor when thread count is nonzero\n");
  }
  self->thread_count = 0;
  NaClRefCountUnref((struct NaClRefCount *) self->iface);
  NaClCondVarDtor(&self->cv);
  NaClMutexDtor(&self->mu);

  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl *)
      &kNaClSimpleRevServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
}

static void NaClReverseServiceAddChannelRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4, "Entered AddChannel\n");
  out_args[0]->u.ival = (uint32_t)(*NACL_VTBL(NaClSimpleRevService, nrsp)->
      ConnectAndSpawnHandlerCb)(&nrsp->base, NULL, (void *) nrsp);
  NaClLog(4, "Leaving AddChannel\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClReverseServiceModuleInitDoneRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(4, "Entered ModuleInitDone: service 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) nrsp);
  NaClLog(4, "ModuleInitDone: invoking StartupInitializationComplete\n");
  (*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
   StartupInitializationComplete)(nrsp->iface);
  NaClLog(4, "Leaving ModuleInitDoneRpc\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClReverseServiceRequestQuotaForWriteRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  char                      *file_id = in_args[0]->arrays.carr;
  int64_t                   offset = in_args[1]->u.lval;
  int64_t                   length = in_args[2]->u.lval;

  NaClLog(4, "Entered RequestQuotaForWriteRpc: 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) nrsp);
  out_args[0]->u.lval = (*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
      RequestQuotaForWrite)(nrsp->iface, file_id, offset, length);
  NaClLog(4, "Leaving RequestQuotaForWriteRpc\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

struct NaClReverseCountingThreadInterface {
  struct NaClThreadInterface  base NACL_IS_REFCOUNT_SUBCLASS;
  struct NaClReverseService   *reverse_service;
};

extern struct NaClThreadInterfaceVtbl
  const kNaClReverseThreadInterfaceVtbl; /* fwd */

int NaClReverseThreadIfCtor_protected(
    struct NaClReverseCountingThreadInterface   *self,
    void                                        *factory_data,
    NaClThreadIfStartFunction                   fn_ptr,
    void                                        *thread_data,
    size_t                                      thread_stack_size) {
  struct NaClReverseService *nrsp = (struct NaClReverseService *) factory_data;

  NaClLog(3, "Entered NaClReverseThreadIfCtor_protected\n");
  if (!NaClThreadInterfaceCtor_protected(
          (struct NaClThreadInterface *) self,
          NaClReverseThreadIfFactoryFn,
          NaClRefCountRef((struct NaClRefCount *) nrsp),
          fn_ptr,
          thread_data,
          thread_stack_size)) {
    NaClLog(4, "NaClThreadInterfaceCtor_protected failed\n");
    NaClRefCountUnref((struct NaClRefCount *) nrsp);
    return 0;
  }

  self->reverse_service = nrsp;
  (*NACL_VTBL(NaClReverseService, nrsp)->ThreadCountIncr)(nrsp);

  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClReverseThreadInterfaceVtbl;

  NaClLog(3, "Leaving NaClAddrSpSquattingThreadIfCtor_protected\n");
  return 1;
}

/*
 * Takes ownership of rev reference.  Caller should Ref() the argument
 * and Unref on failure if caller does not wish to pass ownership.
 */
int NaClReverseThreadIfFactoryFn(
    void                        *factory_data,
    NaClThreadIfStartFunction   fn_ptr,
    void                        *thread_data,
    size_t                      thread_stack_size,
    struct NaClThreadInterface  **out_new_thread) {
  struct NaClReverseCountingThreadInterface *new_thread;
  int                                       rv = 1;

  NaClLog(3, "Entered NaClReverseThreadIfFactoryFn\n");
  new_thread = (struct NaClReverseCountingThreadInterface *)
      malloc(sizeof *new_thread);
  if (NULL == new_thread) {
    rv = 0;
    goto cleanup;
  }

  if (!(rv =
        NaClReverseThreadIfCtor_protected(new_thread,
                                          factory_data,
                                          fn_ptr,
                                          thread_data,
                                          thread_stack_size))) {
    goto cleanup;
  }

  *out_new_thread = (struct NaClThreadInterface *) new_thread;
  new_thread = NULL;

 cleanup:
  free(new_thread);
  NaClLog(3,
          "Leaving NaClReverseThreadIfFactoryFn, rv %d\n",
          rv);
  return rv;
}

void NaClReverseThreadIfDtor(struct NaClRefCount *vself) {
  struct NaClReverseCountingThreadInterface *self =
      (struct NaClReverseCountingThreadInterface *) vself;

  NaClRefCountUnref((struct NaClRefCount *) self->reverse_service);
  self->reverse_service = NULL;
  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

void NaClReverseThreadIfLaunchCallback(struct NaClThreadInterface *self) {
  NaClLog(4,
          ("NaClReverseThreadIfLaunchCallback: thread 0x%"NACL_PRIxPTR
           " is launching\n"),
          (uintptr_t) self);
}

void NaClReverseThreadIfExit(struct NaClThreadInterface   *vself,
                             void                         *exit_code) {
  struct NaClReverseCountingThreadInterface *self =
      (struct NaClReverseCountingThreadInterface *) vself;
  UNREFERENCED_PARAMETER(exit_code);
  NaClLog(4,
          ("NaClReverseThreadIfExit: thread 0x%"NACL_PRIxPTR
           " is exiting\n"),
          (uintptr_t) vself);

  (*NACL_VTBL(NaClReverseService, self->reverse_service)->ThreadCountDecr)(
      self->reverse_service);

  NaClRefCountUnref((struct NaClRefCount *) self);
  NaClThreadExit();
}

struct NaClSrpcHandlerDesc const kNaClReverseServiceHandlers[] = {
  { NACL_REVERSE_CONTROL_ADD_CHANNEL, NaClReverseServiceAddChannelRpc, },
  { NACL_REVERSE_CONTROL_INIT_DONE, NaClReverseServiceModuleInitDoneRpc, },
  { NACL_REVERSE_REQUEST_QUOTA_FOR_WRITE, NaClReverseServiceRequestQuotaForWriteRpc, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};

struct NaClThreadInterfaceVtbl const kNaClReverseThreadInterfaceVtbl = {
  {
    NaClReverseThreadIfDtor,
  },
  NaClThreadInterfaceStartThread,
  NaClReverseThreadIfLaunchCallback,
  NaClReverseThreadIfExit,
};

static void NaClReverseServiceCbBinder(void   *state,
                                       int    server_loop_ret) {
  struct NaClReverseService *self = (struct NaClReverseService *) state;
  UNREFERENCED_PARAMETER(server_loop_ret);

  (*NACL_VTBL(NaClReverseInterface, self->iface)->ReportCrash)(
      self->iface);
}

int NaClStartServiceCb(struct NaClReverseService *self,
                       void (*exit_cb)(void *server_instance_data,
                                       int  server_loop_ret),
                       void *instance_data) {
  int retval = 0; /* fail */

  NaClLog(4, "Entered ReverseSocket::StartService\n");

  NaClLog(4, "StartService: invoking ConnectAndSpawnHandler\n");
  if (0 != (*NACL_VTBL(NaClSimpleRevService, self)->
            ConnectAndSpawnHandlerCb)((struct NaClSimpleRevService *) self,
                                      exit_cb,
                                      instance_data)) {
    NaClLog(LOG_ERROR, "StartServiceCb: ConnectAndSpawnHandlerCb failed\n.");
    goto done;
  }
  retval = 1;

 done:
  NaClLog(4, "Leaving ReverseSocket::StartService\n");
  return retval;
}

int NaClReverseServiceStart(struct NaClReverseService   *self,
                            int                         crash_report) {
  NaClLog(4, "NaClReverseServiceStart: starting service\n");
  if (0 != crash_report) {
    return NaClStartServiceCb(self, NaClReverseServiceCbBinder, (void *) self);
  } else {
    return NaClStartServiceCb(self, NULL, (void *) self);
  }
  return 0;
}

void NaClReverseServiceWaitForServiceThreadsToExit(
    struct NaClReverseService *self) {
  NaClLog(4, "NaClReverseServiceWaitForServiceThreadsToExit\n");
  NaClXMutexLock(&self->mu);
  while (0 != self->thread_count) {
    NaClLog(4, "NaClReverseServiceWaitForServiceThreadsToExit: %d left\n",
            self->thread_count);
    NaClXCondVarWait(&self->cv, &self->mu);
    NaClLog(5, "NaClReverseServiceWaitForServiceThreadsToExit: woke up\n");
  }
  NaClXMutexUnlock(&self->mu);
  NaClLog(4, "NaClReverseServiceWaitForServiceThreadsToExit: all done\n");
}

void NaClReverseServiceThreadCountIncr(
    struct NaClReverseService *self) {
  NaClLog(5, "NaClReverseServiceThreadCountIncr\n");
  NaClXMutexLock(&self->mu);
  if (0 == ++self->thread_count) {
    NaClLog(LOG_FATAL,
            "NaClReverseServiceThreadCountIncr: "
            "thread count overflow!\n");
  }
  NaClXMutexUnlock(&self->mu);
}

void NaClReverseServiceThreadCountDecr(
    struct NaClReverseService *self) {
  NaClLog(5, "NaClReverseServiceThreadCountDecr\n");
  NaClXMutexLock(&self->mu);
  if (0 == self->thread_count) {
    NaClLog(LOG_FATAL,
            "NaClReverseServiceThreadCountDecr: "
            "decrementing thread count when count is zero\n");
  }
  if (0 == --self->thread_count) {
    NaClXCondVarBroadcast(&self->cv);
  }
  NaClXMutexUnlock(&self->mu);
}

struct NaClReverseServiceVtbl const kNaClReverseServiceVtbl = {
  {
    {
      NaClReverseServiceDtor,
    },
    NaClSimpleRevServiceConnectAndSpawnHandler,
    NaClSimpleRevServiceConnectAndSpawnHandlerCb,
    NaClSimpleRevServiceConnectionFactory,
    NaClSimpleRevServiceRpcHandler,
  },
  NaClReverseServiceStart,
  NaClReverseServiceWaitForServiceThreadsToExit,
  NaClReverseServiceThreadCountIncr,
  NaClReverseServiceThreadCountDecr,
};

int NaClReverseInterfaceCtor_protected(
    struct NaClReverseInterface   *self) {
  NaClLog(4, "Entered NaClReverseInterfaceCtor_protected\n");
  if (!NaClRefCountCtor((struct NaClRefCount *) self)) {
    NaClLog(4,
            "NaClReverseInterfaceCtor_protected: "
            "NaClRefCountCtor base class ctor failed\n");
    return 0;
  }
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl const *) &kNaClReverseInterfaceVtbl;
  NaClLog(4,
          "Leaving NaClReverseInterfaceCtor_protected, returning 1\n");
  return 1;
}

void NaClReverseInterfaceDtor(struct NaClRefCount *vself) {
  struct NaClReverseInterface *self =
      (struct NaClReverseInterface *) vself;

  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

void NaClReverseInterfaceStartupInitializationComplete(
    struct NaClReverseInterface *self) {
  NaClLog(3,
          ("NaClReverseInterfaceStartupInitializationComplete(0x%08"
           NACL_PRIxPTR")\n"),
          (uintptr_t) self);
}

void NaClReverseInterfaceReportCrash(
    struct NaClReverseInterface *self) {
  NaClLog(3,
          "NaClReverseInterfaceReportCrash(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) self);
}

int64_t NaClReverseInterfaceRequestQuotaForWrite(
    struct NaClReverseInterface   *self,
    char const                    *file_id,
    int64_t                       offset,
    int64_t                       length) {
  NaClLog(3,
          ("NaClReverseInterfaceRequestQuotaForWrite(0x%08"NACL_PRIxPTR", %s"
           ", %08"NACL_PRId64", %08"NACL_PRId64")\n"),
          (uintptr_t) self, file_id, offset, length);
  return 0;
}

struct NaClReverseInterfaceVtbl const kNaClReverseInterfaceVtbl = {
  {
    NaClReverseInterfaceDtor,
  },
  NaClReverseInterfaceStartupInitializationComplete,
  NaClReverseInterfaceReportCrash,
  NaClReverseInterfaceRequestQuotaForWrite,
};
