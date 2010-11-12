// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/completion_callback_table.h"

#include "gen/native_client/src/shared/ppapi_proxy/ppp_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace ppapi_proxy {

CompletionCallbackTable::CompletionCallbackTable()
  : next_id_(0) {
}

CompletionCallbackTable& CompletionCallbackTable::GetTable() {
  static CompletionCallbackTable table;
  return table;
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
  std::map<int32_t, PP_CompletionCallback>::iterator it =
      table_.find(callback_id);
  if (table_.end() != it) {
    return it->second;
  }
  PP_CompletionCallback rv = { NULL, NULL };
  return rv;
}
}  // namespace ppapi_proxy

// Invokes the PP_CompletionCallback in the plugin-side after it was invoked via
// srpc from the browser-side.
void CompletionCallbackRpcServer::InvokeCompletionCallback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* closure,
      int32_t callback_id,
      int32_t result) {
  NaClSrpcClosureRunner runner(closure);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_CompletionCallback callback =
      ppapi_proxy::CompletionCallbackTable::GetTable().RemoveCallback(
          callback_id);
  if (NULL != callback.func) {
    (*callback.func)(callback.user_data, result);
    rpc->result = NACL_SRPC_RESULT_OK;
  }
}
