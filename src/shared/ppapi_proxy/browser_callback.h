// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_

#include "native_client/src/include/portability.h"

struct NaClSrpcChannel;

namespace ppapi_proxy {

// Data used in the trusted-side of the wire to invoke a completion callback
// on main thread of the untrusted (nexe) side.
//
// The channel will always be the "main channel", the one listened to by the
// main thread of a nexe.  We need to remember the channel in this data
// structure because (for the present at least) there is one nexe (and hence
// one instance of sel_ldr and one channel) per embed tag.

struct RemoteCallbackInfo {
 public:
  NaClSrpcChannel* main_srpc_channel;
  int32_t callback_index;
};

// Calls the remote implementation of a callback.  Callback_info is a pointer
// to an instance of RemoteCallbackInfo, which is deleted when this is invoked.
// This method parallels PP_RunCompletionCallback, and res is used to indicate
// failure (if negative) or success (otherwise).
void InvokeRemoteCallback(void* remote_callback_info, int32_t res);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_

