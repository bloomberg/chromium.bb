// Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
// A plugin-side callback is proxied over to the browser side using
// a |callback_id|. This id is then paired with an |srpc_channel| listened to
// by the nexe that supplied the callback.
//
// |read_buffer| is used with callbacks that are invoked on byte reads.
struct RemoteCallbackInfo {
  NaClSrpcChannel* srpc_channel;
  int32_t callback_id;
  char* read_buffer;
};

// Calls the remote implementation of a callback on the plugin side.
// Implements a PP_CompletionCallback_Func type that can be used along with an
// instance of a RemoteCallbackInfo as |user_data| to provide a
// PP_CompletionCallback to browser functions.
//
// |remote_callback| is a pointer to a RemoteCallbackInfo,
// deleted after rpc via scoped_ptr. The associated |read_buffer| is also
// deleted.
// |result| is passed by the callback invoker to indicate success or error.
// It is passed as-is to the plugin side callback.
void RunRemoteCallback(void* user_data, int32_t result) {
  CHECK(PPBCoreInterface()->IsMainThread());
  DebugPrintf("RunRemoteCallback: result=%"NACL_PRId32"\n", result);
  nacl::scoped_ptr<RemoteCallbackInfo> remote_callback(
      reinterpret_cast<RemoteCallbackInfo*>(user_data));
  nacl::scoped_array<char> read_buffer(remote_callback->read_buffer);

  // If the proxy is down, the channel is no longer usable for remote calls.
  PP_Instance instance =
      LookupInstanceIdForSrpcChannel(remote_callback->srpc_channel);
  if (LookupBrowserPppForInstance(instance) == NULL) {
    DebugPrintf("RunRemoteCallback: proxy=NULL\n", result);
    return;
  }

  nacl_abi_size_t read_buffer_size = 0;
  if (result > 0 && remote_callback->read_buffer != NULL)
    read_buffer_size = static_cast<nacl_abi_size_t>(result);

  CompletionCallbackRpcClient::RunCompletionCallback(
      remote_callback->srpc_channel,
      remote_callback->callback_id,
      result,
      read_buffer_size,
      read_buffer.get());
}

}  // namespace

// Builds a RemoteCallbackInfo and returns PP_CompletionCallback corresponding
// to RunRemoteCallback or NULL on failure.
struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
    int32_t bytes_to_read,
    char** buffer) {
  RemoteCallbackInfo* remote_callback = new(std::nothrow) RemoteCallbackInfo;
  if (remote_callback == NULL)  // new failed.
    return PP_BlockUntilComplete();
  remote_callback->srpc_channel = srpc_channel;
  remote_callback->callback_id = callback_id;
  remote_callback->read_buffer = NULL;

  if (bytes_to_read > 0 && buffer != NULL) {
    *buffer = new(std::nothrow) char[bytes_to_read];
    if (*buffer == NULL)  // new failed.
      return PP_BlockUntilComplete();
    remote_callback->read_buffer = *buffer;
  }

  return PP_MakeCompletionCallback(RunRemoteCallback, remote_callback);
}

struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id) {
  return MakeRemoteCompletionCallback(srpc_channel, callback_id, 0, NULL);
}

void DeleteRemoteCallbackInfo(struct PP_CompletionCallback callback) {
  nacl::scoped_ptr<RemoteCallbackInfo> remote_callback(
      reinterpret_cast<RemoteCallbackInfo*>(callback.user_data));
  nacl::scoped_array<char> read_buffer(remote_callback->read_buffer);
}

}  // namespace ppapi_proxy
