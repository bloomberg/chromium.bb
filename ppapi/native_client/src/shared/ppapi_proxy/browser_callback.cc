// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_callback.h"

#include <new>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

namespace {

bool BytesWereRead(int32_t num_bytes) {
  return (num_bytes > 0);
}

nacl_abi_size_t CastToNaClAbiSize(int32_t result) {
  return static_cast<nacl_abi_size_t>(result);
}

// Data structure used on the browser side to invoke a completion callback
// on the plugin side.
//
// A plugin-side callback is proxied over to the browser side using
// a |callback_id|. This id is then paired with an |srpc_channel| listened to
// by the nexe that supplied the callback.
//
// |read_buffer| is used with callbacks that are invoked on byte reads.
// |check_result_func| is a pointer to a function used to check the
// result of the operation.  The semantics of the result value may be different
// depending on how the callback operation was initiated, and
// |check_result_func| provides an abstraction to the semantics.
// |get_size_read_func| is a pointer to a function used to get the
// number of bytes read.  The way the number of bytes read
// retrieved/calculated may be different depending on how the callback was
// initiated, and |get_size_read_func| provides the indirection.
struct RemoteCallbackInfo {
  NaClSrpcChannel* srpc_channel;
  int32_t callback_id;
  char* read_buffer;
  PP_Var read_var;
  CheckResultFunc check_result_func;
  GetReadSizeFunc get_size_read_func;
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
  CheckResultFunc check_result_func = remote_callback->check_result_func;
  GetReadSizeFunc get_size_read_func = remote_callback->get_size_read_func;
  if ((*check_result_func)(result) && remote_callback->read_buffer != NULL)
    read_buffer_size = (*get_size_read_func)(result);
  if (remote_callback->read_var.type != PP_VARTYPE_NULL) {
    read_buffer_size = kMaxReturnVarSize;
    read_buffer.reset(
        Serialize(&remote_callback->read_var, 1, &read_buffer_size));
    PPBVarInterface()->Release(remote_callback->read_var);
  }

  NaClSrpcError srpc_result =
      CompletionCallbackRpcClient::RunCompletionCallback(
          remote_callback->srpc_channel,
          remote_callback->callback_id,
          result,
          read_buffer_size,
          read_buffer.get());
  DebugPrintf("RunRemoteCallback: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_INTERNAL)
    CleanUpAfterDeadNexe(instance);
}

}  // namespace

// Builds a RemoteCallbackInfo and returns PP_CompletionCallback corresponding
// to RunRemoteCallback or NULL on failure.
struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
    int32_t bytes_to_read,
    char** buffer,
    PP_Var** var,
    CheckResultFunc check_result_func,
    GetReadSizeFunc get_size_read_func) {
  nacl::scoped_ptr<RemoteCallbackInfo> remote_callback(
      new(std::nothrow) RemoteCallbackInfo);
  if (remote_callback.get() == NULL)  // new failed.
    return PP_BlockUntilComplete();
  remote_callback->srpc_channel = srpc_channel;
  remote_callback->callback_id = callback_id;
  remote_callback->read_buffer = NULL;
  remote_callback->read_var = PP_MakeNull();
  remote_callback->check_result_func = check_result_func;
  remote_callback->get_size_read_func = get_size_read_func;

  if (bytes_to_read > 0 && buffer != NULL) {
    *buffer = new(std::nothrow) char[bytes_to_read];
    if (*buffer == NULL)  // new failed.
      return PP_BlockUntilComplete();
    remote_callback->read_buffer = *buffer;
  }
  if (var)
    *var = &remote_callback->read_var;

  return PP_MakeOptionalCompletionCallback(
      RunRemoteCallback, remote_callback.release());
}

struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
    int32_t bytes_to_read,
    char** buffer,
    CheckResultFunc check_result_func,
    GetReadSizeFunc get_size_read_func) {
  return MakeRemoteCompletionCallback(srpc_channel, callback_id, bytes_to_read,
                                      buffer, NULL, check_result_func,
                                      get_size_read_func);
}

struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
    int32_t bytes_to_read,
    char** buffer) {
  return MakeRemoteCompletionCallback(srpc_channel, callback_id, bytes_to_read,
                                      buffer, BytesWereRead, CastToNaClAbiSize);
}

struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
    PP_Var** var) {
  return MakeRemoteCompletionCallback(srpc_channel, callback_id, 0, NULL, var,
                                      BytesWereRead, CastToNaClAbiSize);
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
