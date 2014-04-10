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

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/reverse_service/manifest_rpc.h"
#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/validator/nacl_file_info.h"

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

static void NaClReverseServiceModuleExitRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  int                       exit_status = in_args[0]->u.ival;
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(4,
          ("Entered ModuleExitRpc: service 0x%08"NACL_PRIxPTR
           ", exit_status 0x%d\n"),
          (uintptr_t) nrsp, exit_status);
  NaClLog(4, "ModuleExitRpc: invoking ReportExitStatus\n");
  (*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
   ReportExitStatus)(nrsp->iface, exit_status);
  NaClLog(4, "Leaving ModuleExitRpc\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClReverseServicePostMessageRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  char                      *msg = in_args[0]->arrays.carr;
  nacl_abi_size_t           nbytes = in_args[0]->u.count;

  NaClLog(4, "Entered PostMessageRpc: service 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) nrsp);
  (*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
   DoPostMessage)(nrsp->iface, msg, nbytes);
  out_args[0]->u.ival = nbytes;
  NaClLog(4, "Leaving PostMessageRpc\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClReverseServiceCreateProcessRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  struct NaClDesc           *sock_addr;
  struct NaClDesc           *app_addr;
  int                       status;
  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4,
          "Entered NaClReverseServiceCreateProcessRpc: 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) nrsp);
  status = (*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
            CreateProcess)(nrsp->iface, &sock_addr, &app_addr);
  out_args[0]->u.ival = status;
  out_args[1]->u.hval = (0 == status)
      ? sock_addr
      : (struct NaClDesc *) NaClDescInvalidMake();
  out_args[2]->u.hval = (0 == status)
      ? app_addr
      : (struct NaClDesc *) NaClDescInvalidMake();
  NaClLog(4, "Leaving NaClReverseServiceCreateProcessRpc\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

struct CreateProcessFunctorState {
  struct NaClDesc **out_sock_addr;
  struct NaClDesc **out_app_addr;
  int32_t *out_pid;
  struct NaClSrpcClosure *cls;
};

static void CreateProcessFunctor(void *functor_state,
                                 struct NaClDesc *sock_addr,
                                 struct NaClDesc *app_addr,
                                 int32_t pid) {
  struct CreateProcessFunctorState *state =
      (struct CreateProcessFunctorState *) functor_state;
  if (NULL == sock_addr) {
    sock_addr = (struct NaClDesc *) NaClDescInvalidMake();
  }
  if (NULL == app_addr) {
    app_addr = (struct NaClDesc *) NaClDescInvalidMake();
  }
  *state->out_sock_addr = sock_addr;
  *state->out_app_addr = app_addr;
  *state->out_pid = pid;
  (*state->cls->Run)(state->cls);
}

static void NaClReverseServiceCreateProcessFunctorResultRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  struct CreateProcessFunctorState functor_state;

  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4,
          "Entered NaClReverseServiceCreateProcessFunctorResultRpc: 0x%08"
          NACL_PRIxPTR"\n",
          (uintptr_t) nrsp);
  functor_state.out_sock_addr = &out_args[0]->u.hval;
  functor_state.out_app_addr = &out_args[1]->u.hval;
  functor_state.out_pid = &out_args[2]->u.ival;
  functor_state.cls = done_cls;

  rpc->result = NACL_SRPC_RESULT_OK;
  (*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
   CreateProcessFunctorResult)(nrsp->iface,
                               CreateProcessFunctor, &functor_state);
  NaClLog(4, "Leaving NaClReverseServiceCreateProcessFunctorResultRpc\n");
}

/*
 * Manifest name service, internal APIs.
 *
 * Manifest file lookups result in read-only file descriptors with a
 * handle.  When the descriptor is closed, the service runtime must
 * inform the plugin of this using the handle, so that the File object
 * reference can be closed (thereby allowing the browser to delete or
 * otherwise garbage collect the file).  Files, being from the
 * manifest, cannot be deleted.  The manifest is also a read-only
 * object, so no new entries can be made to it.
 *
 * Read-only proxies do not require quota support per se, since we do
 * not limit read bandwidth.  Quota support is needed for storage
 * limits, though could also be used to limit write bandwidth (prevent
 * disk output saturation, limit malicious code's ability to cause
 * disk failures, especially with flash disks with limited write
 * cycles).
 */

/*
 * Look up by string name, resulting in a handle (if name is in the
 * preimage), a object proxy handle, and an error code.
 */
static void NaClReverseServiceManifestLookupRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClReverseService *nrsp =
    (struct NaClReverseService *) rpc->channel->server_instance_data;
  char                      *url_key = in_args[0]->arrays.str;
  int                       flags = in_args[0]->u.ival;
  struct NaClFileInfo       info;
  struct NaClHostDesc       *host_desc;
  struct NaClDescIoDesc     *io_desc = NULL;
  struct NaClDesc           *nacl_desc = NULL;

  memset(&info, 0, sizeof(info));

  NaClLog(4, "Entered ManifestLookupRpc: 0x%08"NACL_PRIxPTR", %s, %d\n",
          (uintptr_t) nrsp, url_key, flags);

  NaClLog(4, "ManifestLookupRpc: invoking OpenManifestEntry\n");
  if (!(*NACL_VTBL(NaClReverseInterface, nrsp->iface)->
        OpenManifestEntry)(nrsp->iface, url_key, &info)
      || -1 == info.desc) {
    NaClLog(1, "ManifestLookupRpc: OpenManifestEntry failed.\n");
    out_args[0]->u.ival = NACL_ABI_ENOENT; /* failed */
    out_args[1]->u.hval = (struct NaClDesc *) NaClDescInvalidMake();
    out_args[2]->u.lval = 0;
    out_args[3]->u.lval = 0;
    out_args[4]->u.count = 0;
    goto done;
  }
  NaClLog(4, "ManifestLookupRpc: OpenManifestEntry returned desc %d.\n",
          info.desc);
  host_desc = (struct NaClHostDesc *) malloc(sizeof *host_desc);
  CHECK(host_desc != NULL);
  CHECK(NaClHostDescPosixTake(host_desc, info.desc, NACL_ABI_O_RDONLY) == 0);
  io_desc = NaClDescIoDescMake(host_desc);
  CHECK(io_desc != NULL);
  nacl_desc = (struct NaClDesc *) io_desc;

  out_args[0]->u.ival = 0;  /* OK */
  out_args[1]->u.hval = nacl_desc;
  out_args[2]->u.lval = (int64_t) info.file_token.lo;
  out_args[3]->u.lval = (int64_t) info.file_token.hi;
  out_args[4]->u.count = 10;
  strncpy(out_args[4]->arrays.carr, "123456789", 10);
  /*
   * TODO(phosek): the array should be an object reference (issue 3035).
   */

 done:
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
  NaClDescSafeUnref((struct NaClDesc *) io_desc);
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
  { NACL_REVERSE_CONTROL_REPORT_STATUS, NaClReverseServiceModuleExitRpc, },
  { NACL_REVERSE_CONTROL_POST_MESSAGE, NaClReverseServicePostMessageRpc, },
  { NACL_REVERSE_CONTROL_CREATE_PROCESS, NaClReverseServiceCreateProcessRpc, },
  { NACL_REVERSE_CONTROL_CREATE_PROCESS_INTERLOCKED,
    NaClReverseServiceCreateProcessFunctorResultRpc, },
  { NACL_MANIFEST_LOOKUP, NaClReverseServiceManifestLookupRpc, },
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

int NaClReverseInterfaceOpenManifestEntry(
    struct NaClReverseInterface   *self,
    char const                    *url_key,
    struct NaClFileInfo           *info) {
  NaClLog(3,
          ("NaClReverseInterfaceOpenManifestEntry(0x%08"NACL_PRIxPTR", %s"
           ", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) self, url_key, (uintptr_t) info);
  return 0;
}

void NaClReverseInterfaceReportCrash(
    struct NaClReverseInterface *self) {
  NaClLog(3,
          "NaClReverseInterfaceReportCrash(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) self);
}

void NaClReverseInterfaceReportExitStatus(
    struct NaClReverseInterface   *self,
    int                           exit_status) {
  NaClLog(3,
          "NaClReverseInterfaceReportExitStatus(0x%08"NACL_PRIxPTR", 0x%x)\n",
          (uintptr_t) self, exit_status);
}

void NaClReverseInterfaceDoPostMessage(
    struct NaClReverseInterface   *self,
    char const                    *message,
    size_t                        message_bytes) {
  NaClLog(3,
          ("NaClReverseInterfaceDoPostMessage(0x%08"NACL_PRIxPTR", %s"
           ", %08"NACL_PRIdS")\n"),
          (uintptr_t) self, message, message_bytes);
}

int NaClReverseInterfaceCreateProcess(
    struct NaClReverseInterface   *self,
    struct NaClDesc               **out_sock_addr,
    struct NaClDesc               **out_app_addr) {
  NaClLog(3,
          ("NaClReverseInterfaceCreateProcess(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) self,
          (uintptr_t) out_sock_addr,
          (uintptr_t) out_app_addr);
  return -NACL_ABI_EAGAIN;
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

void NaClReverseInterfaceCreateProcessFunctorResult(
    struct NaClReverseInterface *self,
    void (*result_functor)(void *functor_state,
                           struct NaClDesc *out_sock_addr,
                           struct NaClDesc *out_app_addr,
                           int32_t out_pid_or_errno),
    void *functor_state) {
  NaClLog(3,
          ("NaClReverseInterfaceCreateProcessFunctorResult(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) self,
          (uintptr_t) result_functor,
          (uintptr_t) functor_state);
}

void NaClReverseInterfaceFinalizeProcess(struct NaClReverseInterface *self,
                                         int32_t pid) {
  NaClLog(3,
          ("NaClReverseInterfaceFinalizeProcess(0x%08"NACL_PRIxPTR", %d)\n"),
          (uintptr_t) self, pid);
}

struct NaClReverseInterfaceVtbl const kNaClReverseInterfaceVtbl = {
  {
    NaClReverseInterfaceDtor,
  },
  NaClReverseInterfaceStartupInitializationComplete,
  NaClReverseInterfaceOpenManifestEntry,
  NaClReverseInterfaceReportCrash,
  NaClReverseInterfaceReportExitStatus,
  NaClReverseInterfaceDoPostMessage,
  NaClReverseInterfaceCreateProcess,
  NaClReverseInterfaceCreateProcessFunctorResult,
  NaClReverseInterfaceFinalizeProcess,
  NaClReverseInterfaceRequestQuotaForWrite,
};
