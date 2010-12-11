// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_callback.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "srpcgen/ppp_rpc.h"


namespace ppapi_proxy {

// InvokeRemoteCallback invokes the PP_CompletionCallback on the untrusted
// (nexe) side from the browser-side.  This method may only successfully be
// invoked from the browser/renderer's PPAPI main thread.
void InvokeRemoteCallback(void* remote_callback_info, int32_t result) {
  nacl::scoped_ptr<RemoteCallbackInfo>
      data(reinterpret_cast<RemoteCallbackInfo*>(remote_callback_info));
  if (result < 0) {
    // Method was invoked with an error condition.  Clean up and return.
    return;
  }
  if (PPBCoreInterface()->IsMainThread()) {
    // We cannot run the closure on other than the main thread.
    DebugPrintf("ERROR: CompletionCallbackInvoker run off the main thread.\n");
    return;
  }
  if (data != NULL) {
    CompletionCallbackRpcClient::InvokeCompletionCallback(
        data->main_srpc_channel,
        data->callback_index,
        result);
  }
}

}  // namespace ppapi_proxy
