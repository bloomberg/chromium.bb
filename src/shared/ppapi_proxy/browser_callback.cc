// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_callback.h"

#include <new>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

namespace {

// Data structure used on the browser side to invoke a completion callback
// on the plugin side.
//
// A plugin-side callback is proxied over to the browser side using an id.
// This id is then paired with a channel listened to by the nexe that supplied
// the callback.
struct RemoteCallbackInfo {
 public:
  NaClSrpcChannel* srpc_channel;
  int32_t callback_id;
};

// Calls the remote implementation of a callback on the plugin side.
// Implements a PP_CompletionCallback_Func type that can be used along with an
// instance of a RemoteCallbackInfo as |user_data| to provide a
// PP_CompletionCallback to browser functions.
//
// |remote_callback| is a pointer to a RemoteCallbackInfo,
// deleted after rpc via scoped_ptr.
// |result| is passed by the callback invoker to indicate success or error.
// It is passed as-is to the plugin side callback.
void RunRemoteCallback(void* user_data, int32_t result) {
  CHECK(PPBCoreInterface()->IsMainThread());
  DebugPrintf("RunRemotecallback: result=%"NACL_PRId32"\n", result);
  nacl::scoped_ptr<RemoteCallbackInfo> remote_callback(
      reinterpret_cast<RemoteCallbackInfo*>(user_data));
  CompletionCallbackRpcClient::RunCompletionCallback(
      remote_callback->srpc_channel,
      remote_callback->callback_id,
      result);
}

}  // namespace

// Builds a RemoteCallbackInfo and returns PP_CompletionCallback corresponding
// to RunRemoteCallback or NULL on failure.
struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id) {
  RemoteCallbackInfo* remote_callback = new(std::nothrow) RemoteCallbackInfo;
  if (remote_callback == NULL)
    return PP_BlockUntilComplete();
  remote_callback->srpc_channel = srpc_channel;
  remote_callback->callback_id = callback_id;
  return PP_MakeCompletionCallback(RunRemoteCallback, remote_callback);
}

void DeleteRemoteCallbackInfo(struct PP_CompletionCallback callback) {
  nacl::scoped_ptr<RemoteCallbackInfo> remote_callback(
      reinterpret_cast<RemoteCallbackInfo*>(callback.user_data));
}

}  // namespace ppapi_proxy
