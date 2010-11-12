// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gen/native_client/src/shared/ppapi_proxy/ppp_rpc.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/completion_callback_table.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace ppapi_proxy {
// CompletionCallbackInvoker invokes the Completioncallback on the plugin-side
// of the wire from the browser-side.
void CompletionCallbackInvoker(void* arg, int32_t result) {
  nacl::scoped_ptr<CallbackInvokerData>
      data(reinterpret_cast<CallbackInvokerData*>(arg));
  if (data != NULL) {
    CompletionCallbackRpcClient::InvokeCompletionCallback(data->channel(),
                                                          data->callback_id(),
                                                          result);
  }
}
}  // namespace ppapi_proxy
