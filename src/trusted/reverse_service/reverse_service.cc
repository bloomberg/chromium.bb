/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/reverse_service/reverse_service.h"

#include "native_client/src/include/nacl_compiler_annotations.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace {

void Test(NaClSrpcRpc* rpc,
          NaClSrpcArg** in_args,
          NaClSrpcArg** out_args,
          NaClSrpcClosure* done) {
  char *msg = in_args[0]->arrays.str;
  UNREFERENCED_PARAMETER(out_args);
  // use rpc->channel rather than rpc->channel->server_instance_data
  // to show that Test RPCs arrive in different channels.
  NaClLog(0, "Test: [%"NACL_PRIxPTR"] %s\n",
          reinterpret_cast<uintptr_t>(rpc->channel), msg);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void AddChannel(NaClSrpcRpc* rpc,
                NaClSrpcArg** in_args,
                NaClSrpcArg** out_args,
                NaClSrpcClosure* done) {
  nacl::ReverseService* service = reinterpret_cast<nacl::ReverseService*>(
      rpc->channel->server_instance_data);
  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);
  NaClLog(4, "Entered AddChannel\n");
  out_args[0]->u.bval = service->Start();
  NaClLog(4, "Leaving AddChannel\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void RevLog(NaClSrpcRpc* rpc,
            NaClSrpcArg** in_args,
            NaClSrpcArg** out_args,
            NaClSrpcClosure* done) {
  nacl::ReverseService* service = reinterpret_cast<nacl::ReverseService*>(
      rpc->channel->server_instance_data);
  UNREFERENCED_PARAMETER(out_args);
  char* message = in_args[0]->arrays.str;

  if (NULL == service->reverse_interface()) {
    NaClLog(1, "Log RPC, no reverse_interface.  Message: %s\n", message);
  } else {
    service->reverse_interface()->Log(message);
  }
  done->Run(done);
}

}  // namespace

namespace nacl {

// need NaClThreadIfFactoryFunction that keeps "this" to do counting

struct ReverseCountingThreadInterface {
  struct NaClThreadInterface  base;
  nacl::ReverseService* rev;
};

/* fwd */ extern NaClThreadInterfaceVtbl const kReverseThreadInterfaceVtbl;

/*
 * Takes ownership of rev reference.  Caller should Ref() the argument
 * and Unref on failure if caller does not wish to pass ownership.
 */
int ReverseThreadIfFactoryFn(
    void* factory_data,
    NaClThreadIfStartFunction fn_ptr,
    void* thread_data,
    size_t thread_stack_size,
    NaClThreadInterface** out_new_thread) {
  ReverseService* rev = reinterpret_cast<ReverseService*>(factory_data);
  ReverseCountingThreadInterface* new_thread;
  int rv;

  NaClLog(4, "ReverseService::ReverseThreadIfFactoryFn\n");
  new_thread = reinterpret_cast<ReverseCountingThreadInterface*>(
      malloc(sizeof *new_thread));
  if (NULL == new_thread) {
    return 0;
  }
  new_thread->rev = rev;
  rev->IncrThreadCount();
  if (0 != (rv =
            NaClThreadInterfaceThreadPlacementFactory(
                ReverseThreadIfFactoryFn,
                reinterpret_cast<void*>(rev->Ref()),
                fn_ptr,
                thread_data,
                thread_stack_size,
                reinterpret_cast<NaClRefCountVtbl const*>(
                    &kReverseThreadInterfaceVtbl),
                reinterpret_cast<NaClThreadInterface*>(new_thread)))) {
    *out_new_thread = reinterpret_cast<NaClThreadInterface*>(new_thread);
    new_thread = NULL;
  } else {
    NaClLog(4, "ReverseService::ReverseThreadIfFactoryFn FAILED TO LAUNCH\n");
    rev->DecrThreadCount();
  }
  free(new_thread);
  return rv;
}

void ReverseThreadIfDtor(struct NaClRefCount* vself) {
  ReverseCountingThreadInterface* self =
      (ReverseCountingThreadInterface*) vself;

  self->rev->Unref();
  self->rev = NULL;

  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

void ReverseThreadIfLaunchCallback(struct NaClThreadInterface* self) {
  NaClLog(4,
          ("ReverseService::ReverseThreadIfLaunchCallback: thread 0x%"
           NACL_PRIxPTR" is launching\n"),
          (uintptr_t) self);
}

void ReverseThreadIfExit(struct NaClThreadInterface* vself,
                         void* exit_code) {
  ReverseCountingThreadInterface *self =
      reinterpret_cast<ReverseCountingThreadInterface*>(vself);
  NaClLog(4,
          ("ReverseService::ReverseThreadIfExit: thread 0x%"NACL_PRIxPTR
           " is exiting\n"),
          (uintptr_t) vself);

  self->rev->DecrThreadCount();

  NaClRefCountUnref(reinterpret_cast<struct NaClRefCount*>(self));
  NaClThreadExit(static_cast<int>(reinterpret_cast<uintptr_t>(exit_code)));
}

NaClThreadInterfaceVtbl const kReverseThreadInterfaceVtbl = {
  {
    ReverseThreadIfDtor,
  },
  ReverseThreadIfLaunchCallback,
  ReverseThreadIfExit,
};

NaClSrpcHandlerDesc const ReverseService::handlers[] = {
  { "test:s:", Test, },
  { "revlog:s:", RevLog, },
  { "add_channel::b", AddChannel, },
  { NULL, NULL, },
};

ReverseService::ReverseService(nacl::DescWrapper* conn_cap,
                               ReverseInterface* rif)
    : service_socket_(NULL),
      reverse_interface_(rif),
      thread_count_(0) {
  /*
   * We wait for service threads to exit before dtor'ing, so the
   * reference to this passed to the ctor for the service_socket_ for
   * use by the thread factory will have a longer lifetime than this
   * object.
   */
  NaClLog(4, "ReverseService::ReverseService ctor invoked\n");
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cv_);
  service_socket_.reset(new ReverseSocket(conn_cap,
                                          handlers,
                                          ReverseThreadIfFactoryFn,
                                          reinterpret_cast<void*>(this)));
}

ReverseService::~ReverseService() {
  if (thread_count_ != 0) {
    NaClLog(LOG_FATAL, "ReverseService dtor when thread count is nonzero\n");
  }
  NaClCondVarDtor(&cv_);
  NaClMutexDtor(&mu_);
}


bool ReverseService::Start() {
  return service_socket_->StartService(reinterpret_cast<void*>(this));
}

void ReverseService::WaitForServiceThreadsToExit() {
  NaClLog(4, "ReverseService::WaitForServiceThreadsToExit\n");
  NaClXMutexLock(&mu_);
  while (0 != thread_count_) {
    NaClXCondVarWait(&cv_, &mu_);
    NaClLog(5, "ReverseService::WaitForServiceThreadsToExit: woke up\n");
  }
  NaClXMutexUnlock(&mu_);
  NaClLog(4, "ReverseService::WaitForServiceThreadsToExit ALL DONE\n");
}

void ReverseService::IncrThreadCount() {
  NaClLog(5, "ReverseService::IncrThreadCount\n");
  NaClXMutexLock(&mu_);
  if (0 == ++thread_count_) {
    NaClLog(LOG_FATAL,
            "ReverseService::IncrThreadCount Thread count overflow!\n");
  }
  NaClXMutexUnlock(&mu_);
}

void ReverseService::DecrThreadCount() {
  NaClLog(5, "ReverseService::DecrThreadCount\n");
  NaClXMutexLock(&mu_);
  if (0 == thread_count_) {
    NaClLog(LOG_FATAL,
            ("ReverseService::DecrThreadCount:"
             " Decrementing thread count when count is zero\n"));
  }
  if (0 == --thread_count_) {
    NaClXCondVarBroadcast(&cv_);
  }
  NaClXMutexUnlock(&mu_);
}

}  // namespace nacl
