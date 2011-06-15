/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/trusted/reverse_service/reverse_service.h"

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"

namespace {

void Test(NaClSrpcRpc* rpc,
          NaClSrpcArg** in_args,
          NaClSrpcArg** out_args,
          NaClSrpcClosure* done) {
  char *msg = in_args[0]->arrays.str;
  UNREFERENCED_PARAMETER(out_args);
  // use rpc->channel rather than rpc->channel->server_instance_data
  // to show that Test RPCs arrive in different channels.
  NaClLog(1, "Test: [%"NACL_PRIxPTR"] %s\n",
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

// Manifest name service, internal APIs.
//
// Manifest file lookups result in read-only file descriptors with a
// handle.  When the descriptor is closed, the service runtime must
// inform the plugin of this using the handle, so that the File object
// reference can be closed (thereby allowing the browser to delete or
// otherwise garbage collect the file).  Files, being from the
// manifest, cannot be deleted.  The manifest is also a read-only
// object, so no new entries can be made to it.
//
// Read-only proxies do not require quota support per se, since we do
// not limit read bandwidth.  Quota support is needed for storage
// limits, though could also be used to limit write bandwidth (prevent
// disk output saturation, limit malicious code's ability to cause
// disk failures, especially with flash disks with limited write
// cycles).

// NACL_MANIFEST_LIST list::C -- enumerate all names in the manifest
void ManifestListRpc(NaClSrpcRpc* rpc,
                     NaClSrpcArg** in_args,
                     NaClSrpcArg** out_args,
                     NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(in_args);
  // Placeholder.  This RPC handler will be replaced with code that
  // actually do manifest listing.
  //
  // TODO(bsy) hook up to real manifest info
  out_args[0]->u.count = SNPRINTF(out_args[0]->arrays.carr,
                                  out_args[0]->u.count,
                                  "This is a reply from the manifest reverse"
                                  " service in the plugin.");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// NACL_MANIFEST_LOOKUP lookup:si:ihC -- look up by string name,
// resulting in a handle (if name is in the preimage), a object proxy
// handle, and an error code.
void ManifestLookupRpc(NaClSrpcRpc* rpc,
                       NaClSrpcArg** in_args,
                       NaClSrpcArg** out_args,
                       NaClSrpcClosure* done) {
  char* fname = in_args[0]->arrays.str;
  int flags = in_args[0]->u.ival;

  NaClLog(0, "ManifestLookupRpc: %s, %d\n", fname, flags);
  out_args[0]->u.ival = 0;  // ok
  out_args[1]->u.hval = (struct NaClDesc*) NaClDescInvalidMake();
  // Placeholder.  This RPC handler will be replaced with code that
  // actually do lookups/URL fetches.
  //
  // TODO(bsy): hook up to real name resolution and return a real
  // descriptor.
  out_args[2]->u.count = 10;
  strncpy(out_args[2]->arrays.carr, "123456789", 10);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// NACL_MANIFEST_UNREF unref:C:i -- dereferences the file by object
// proxy handle.  The file descriptor should have been closed.
void ManifestUnrefRpc(NaClSrpcRpc* rpc,
                      NaClSrpcArg** in_args,
                      NaClSrpcArg** out_args,
                      NaClSrpcClosure* done) {
  char* proxy_handle = in_args[0]->arrays.carr;

  NaClLog(0, "ManifestUnrefRpc: %s\n", proxy_handle);
  // Placeholder.  This RPC will be replaced by real code that
  // looks up the object proxy handle to close the Pepper file object.
  //
  // TODO(bsy): replace with real code.
  out_args[0]->u.ival = 0;  // ok
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

}  // namespace

namespace nacl {

// need NaClThreadIfFactoryFunction that keeps "this" to do counting

struct ReverseCountingThreadInterface {
  struct NaClThreadInterface base NACL_IS_REFCOUNT_SUBCLASS;
  nacl::ReverseService* rev;
};

/* fwd */ extern NaClThreadInterfaceVtbl const kReverseThreadInterfaceVtbl;

int ReverseThreadIfFactoryFn(
    void* factory_data,
    NaClThreadIfStartFunction fn_ptr,
    void* thread_data,
    size_t thread_stack_size,
    NaClThreadInterface** out_new_thread);  // fwd

int ReverseThreadIfCtor_protected(
    ReverseCountingThreadInterface* self,
    void* factory_data,
    NaClThreadIfStartFunction fn_ptr,
    void* thread_data,
    size_t thread_stack_size) {
  ReverseService* rev = reinterpret_cast<ReverseService*>(factory_data);

  if (!NaClThreadInterfaceCtor_protected(
          reinterpret_cast<NaClThreadInterface*>(self),
          ReverseThreadIfFactoryFn,
          reinterpret_cast<void*>(rev->Ref()),
          fn_ptr,
          thread_data,
          thread_stack_size)) {
    NaClLog(4,
            ("ReverseService::ReverseThreadIfFactoryFn:"
             " placement base class ctor failed\n"));
    return 0;
  }
  self->rev = rev;
  rev->IncrThreadCount();
  NACL_VTBL(NaClRefCount, self) =
      reinterpret_cast<NaClRefCountVtbl const*>(&kReverseThreadInterfaceVtbl);
  return 1;
}

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
  NaClLog(4, "ReverseService::ReverseThreadIfFactoryFn\n");

  nacl::scoped_ptr_malloc<ReverseCountingThreadInterface> new_thread;

  new_thread.reset(reinterpret_cast<ReverseCountingThreadInterface*>(
      malloc(sizeof *new_thread.get())));
  if (NULL == new_thread.get()) {
    return 0;
  }
  if (!ReverseThreadIfCtor_protected(new_thread.get(),
                                     factory_data,
                                     fn_ptr,
                                     thread_data,
                                     thread_stack_size)) {
    return 0;
  }
  *out_new_thread = reinterpret_cast<NaClThreadInterface*>(
      new_thread.release());
  return 1;
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
  NaClThreadInterfaceStartThread,
  ReverseThreadIfLaunchCallback,
  ReverseThreadIfExit,
};

NaClSrpcHandlerDesc const ReverseService::handlers[] = {
  { NACL_REVERSE_CONTROL_TEST, Test, },
  { NACL_REVERSE_CONTROL_LOG, RevLog, },
  { NACL_REVERSE_CONTROL_ADD_CHANNEL, AddChannel, },
  { NACL_MANIFEST_LIST, ManifestListRpc, },
  { NACL_MANIFEST_LOOKUP, ManifestLookupRpc, },
  { NACL_MANIFEST_UNREF, ManifestUnrefRpc, },
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
  NaClLog(4, "Entered ReverseService::Start\n");
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
