// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

CompletionCallbackTable::CompletionCallbackTable()
    : next_id_(1) {
}

int32_t CompletionCallbackTable::AddCallback(
    const PP_CompletionCallback& callback) {
  if (callback.func == NULL)
    return 0;
  table_[next_id_] = callback;
  return next_id_++;
}

PP_CompletionCallback CompletionCallbackTable::RemoveCallback(
    int32_t callback_id) {
  CallbackTable::iterator it = table_.find(callback_id);
  if (table_.end() != it) {
    PP_CompletionCallback callback = it->second;
    table_.erase(it);
    return callback;
  }
  return PP_BlockUntilComplete();
}

}  // namespace ppapi_proxy

// SRPC-abstraction wrapper around a PP_CompletionCallback.
void CompletionCallbackRpcServer::RunCompletionCallback(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t callback_id,
    int32_t result) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_CompletionCallback callback =
      ppapi_proxy::CompletionCallbackTable::Get()->RemoveCallback(callback_id);
  if (callback.func == NULL)
    return;
  PP_RunCompletionCallback(&callback, result);
  rpc->result = NACL_SRPC_RESULT_OK;
}
