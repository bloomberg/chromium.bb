// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_upcall.h"

#include <new>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "gen/native_client/src/shared/ppapi_proxy/upcall.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_core.h"

// Support for "upcalls" -- RPCs to the browser that are done from other than
// the main thread.  These calls are synchronized by the ppapi_proxy library
// at the plugin end.

using nacl::DescWrapper;
using nacl::DescWrapperFactory;

namespace {

class UserData {
 public:
  UserData(uint32_t number, NaClSrpcChannel* channel) {
    number_ = number;
    channel_ = channel;
  }
  uint32_t number() const { return number_; }
  NaClSrpcChannel* channel() const { return channel_; }

 private:
  uint32_t number_;
  NaClSrpcChannel* channel_;
};

// Plugin-side calls to CallOnMainThread are sent as RPCs to the
// browser.  The browser responds to the RPC by putting the following thunk on
// the the browser's message queue.  The browser will call it back on the
// browser's NPAPI thread, so it is safe to invoke an NPNavigatorRpc interface
// from here.
static void CallOnMainThreadThunk(void* arg, int32_t res) {
  nacl::scoped_ptr<UserData> closure(reinterpret_cast<UserData*>(arg));
  UNREFERENCED_PARAMETER(res);
  if (closure != NULL) {
    // TODO(sehr): Send an RPC back to the NaCl module to run it's closure.
    ppapi_proxy::DebugPrintf("Closure run was invoked!\n");
  }
}

// Structure for passing information to the thread.  Shares ownership of
// the descriptor with the creating routine.  This allows passing ownership
// to the upcall thread.
class UpcallInfo {
 public:
  UpcallInfo(DescWrapper* wrapper, NaClSrpcChannel* channel) {
    wrapper_ = wrapper;
    channel_ = channel;
  }
  ~UpcallInfo() {
    delete wrapper_;
  }
  DescWrapper* wrapper() const { return wrapper_; }
  NaClSrpcChannel* channel() const { return channel_; }

 private:
  DescWrapper* wrapper_;
  NaClSrpcChannel* channel_;
};

static void WINAPI UpcallThread(void* arg) {
  // The memory for info was allocated on the creating (browser UI) thread,
  // but ownership was conferred to the upcall thread for deletion.
  nacl::scoped_ptr<UpcallInfo> info(reinterpret_cast<UpcallInfo*>(arg));
  NaClSrpcServerLoop(info->wrapper()->desc(),
                     PpbUpcalls::srpc_methods,
                     info->channel());
}

}  // namespace

namespace ppapi_proxy {

DescWrapper* BrowserUpcall::Start(struct NaClThread* nacl_thread,
                                  NaClSrpcChannel* upcall_channel) {
  // Create a socket pair for the upcall server.
  DescWrapperFactory factory;
  DescWrapper* pair[2] = { NULL, NULL };
  if (factory.MakeSocketPair(pair)) {
    return NULL;
  }
  nacl::scoped_ptr<DescWrapper> browser_end(pair[0]);
  nacl::scoped_ptr<DescWrapper> plugin_end(pair[1]);
  // Create an info node to pass to the thread.
  nacl::scoped_ptr<UpcallInfo> info(new UpcallInfo(browser_end.get(),
                                                   upcall_channel));
  if (info == NULL) {
    return NULL;
  }
  // On success, info took ownership of browser_end.
  browser_end.release();
  // Create a thread and an SRPC "upcall" server.
  const int kThreadStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(nacl_thread,
                                UpcallThread,
                                info.get(),
                                kThreadStackSize)) {
    return NULL;
  }
  // On successful thread creation, ownership of info passes to the thread.
  info.release();
  // On successful return, the caller gets the plugin_end of the socketpair.
  return plugin_end.release();
}

}  // namespace ppapi_proxy

// Implementation of the upcall RPC service.

void PppUpcallRpcServer::PPP_Core_CallOnMainThread(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t closure_number,
    int32_t delay_in_milliseconds) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Create a PP_CompletionCallback to place on the main thread.
  // Ownership of user_data is transferred via the closure to the main thread.
  UserData* user_data = new UserData(
      static_cast<uint32_t>(closure_number),
      reinterpret_cast<NaClSrpcChannel*>(rpc->channel->server_instance_data));
  PP_CompletionCallback completion_callback =
      PP_MakeCompletionCallback(CallOnMainThreadThunk, user_data);
  // Enqueue the closure.
  ppapi_proxy::CoreInterface()->CallOnMainThread(delay_in_milliseconds,
                                                 completion_callback,
                                                 0);
  rpc->result = NACL_SRPC_RESULT_OK;
}
