// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include <string.h>
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

CompletionCallbackTable::CompletionCallbackTable()
    : next_id_(1) {
}

int32_t CompletionCallbackTable::AddCallback(
    const PP_CompletionCallback& callback,
    char* read_buffer) {
  if (callback.func == NULL)
    return 0;
  int32_t callback_id = next_id_;
  ++next_id_;
  CallbackInfo info = { callback, read_buffer };
  table_[callback_id] = info;
  return callback_id;
}

int32_t CompletionCallbackTable::AddCallback(
    const PP_CompletionCallback& callback) {
  return AddCallback(callback, NULL);
}

PP_CompletionCallback CompletionCallbackTable::RemoveCallback(
    int32_t callback_id, char** read_buffer) {
  CallbackTable::iterator it = table_.find(callback_id);
  if (table_.end() != it) {
    CallbackInfo info = it->second;
    table_.erase(it);
    if (read_buffer != NULL)
      *read_buffer = info.read_buffer;
    return info.callback;
  }
  *read_buffer = NULL;
  return PP_BlockUntilComplete();
}

}  // namespace ppapi_proxy

// SRPC-abstraction wrapper around a PP_CompletionCallback.
void CompletionCallbackRpcServer::RunCompletionCallback(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    int32_t callback_id,
    int32_t result,
    // TODO(polina): use shm for read buffer
    nacl_abi_size_t read_buffer_size, char* read_buffer) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  char* user_buffer;
  PP_CompletionCallback callback =
      ppapi_proxy::CompletionCallbackTable::Get()->RemoveCallback(
          callback_id, &user_buffer);
  if (callback.func == NULL)
    return;

  if (user_buffer != NULL && read_buffer_size > 0)
    memcpy(user_buffer, read_buffer, read_buffer_size);
  PP_RunCompletionCallback(&callback, result);

  rpc->result = NACL_SRPC_RESULT_OK;
}
