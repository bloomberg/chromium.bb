// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/npruntime/npupcall_server.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/npmodule.h"

#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "gen/native_client/src/shared/npruntime/npupcall_rpc.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

// Support for "upcalls" -- RPCs to the browser that are done from other than
// the NPAPI thread.  These calls are synchronized by the npruntime library
// at the caller end.

using nacl::DescWrapper;
using nacl::NPModule;
using nacl::WireFormatToNPP;

namespace {

class NppClosure {
 public:
  NppClosure(uint32_t number, NPModule* module) {
    number_ = number;
    module_ = module;
  }
  uint32_t number() const { return number_; }
  NPModule* module() const { return module_; }

 private:
  uint32_t number_;
  NPModule* module_;
};

// NaCl-side calls to NPN_PluginThreadAsyncCall are sent as RPCs to the
// browser.  The browser responds to the RPC by putting the following thunk on
// the the browser's message queue.  The browser will call it back on the
// browser's NPAPI thread, so it is safe to invoke an NPNavigatorRpc interface
// from here.
static void BrowserAsyncCallThunk(void* arg) {
  NppClosure* closure = reinterpret_cast<NppClosure*>(arg);
  if (NULL != closure) {
    // Send an RPC back to the NaCl module to perform run it's closure.
    NPNavigatorRpcClient::DoAsyncCall(closure->module()->channel(),
                                      closure->number());
  }
  delete closure;
}

// Structure for passing information to the thread.  Shares ownership of
// the descriptor with the creating routine.  This allows passing ownership
// to the upcall thread.
class UpcallInfo {
 public:
  UpcallInfo(DescWrapper* wrapper, NPModule* module) {
    wrapper_ = wrapper;
    module_ = module;
  }
  ~UpcallInfo() {
    delete wrapper_;
  }
  DescWrapper* wrapper() const { return wrapper_; }
  NPModule* module() const { return module_; }

 private:
  DescWrapper* wrapper_;
  NPModule* module_;
};

static void WINAPI UpcallThread(void* arg) {
  UpcallInfo* info = reinterpret_cast<UpcallInfo*>(arg);
  // Run the SRPC server.
  NaClSrpcServerLoop(info->wrapper()->desc(),
                     NPUpcallRpcs::srpc_methods,
                     info->module());
  // Free the info node.
  delete info;
}

}  // namespace

namespace nacl {

DescWrapper* NPUpcallServer::Start(NPModule* module,
                                   struct NaClThread* nacl_thread) {
  DescWrapperFactory factory;
  DescWrapper* pair[2] = { NULL, NULL };
  UpcallInfo* info = NULL;
  DescWrapper* desc = NULL;

  // Create a socket pair for the upcall server.
  if (factory.MakeSocketPair(pair)) {
    goto done;
  }
  // Create an info node to pass to the thread.
  info = new(std::nothrow) UpcallInfo(pair[0], module);
  if (NULL == info) {
    goto done;
  }
  // info takes ownership of pair[0].
  pair[0] = NULL;
  // Create a thread and an SRPC "upcall" server.
  if (!NaClThreadCreateJoinable(nacl_thread, UpcallThread, info, 128 << 10)) {
    goto done;
  }
  // On success, ownership of info passes to the thread.
  info = NULL;
  // On successful return, the caller takes ownership of pair[1].
  desc = pair[1];
  pair[1] = NULL;

 done:
  delete pair[0];
  delete pair[1];
  delete info;
  return desc;
}

}  // namespace nacl

// Implementation of the upcall RPC service.

void NPUpcallRpcServer::NPN_PluginThreadAsyncCall(NaClSrpcRpc* rpc,
                                                  NaClSrpcClosure* done,
                                                  int32_t wire_npp,
                                                  int32_t closure_number) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  NPModule* module = NPModule::GetModule(wire_npp);
  uint32_t number = static_cast<uint32_t>(closure_number);

  // Place a closure on the browser's NPAPI thread.
  NppClosure* closure = new(std::nothrow) NppClosure(number, module);
  if (NULL == closure) {
    return;
  }
  ::NPN_PluginThreadAsyncCall(npp,
                              BrowserAsyncCallThunk,
                              static_cast<void*>(closure));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPUpcallRpcServer::Device3DFlush(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      int32_t wire_npp,
                                      int32_t put_offset,
                                      int32_t* get_offset,
                                      int32_t* token,
                                      int32_t* error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DFlush(WireFormatToNPP(wire_npp),
                                      put_offset,
                                      get_offset,
                                      token,
                                      error);
}
