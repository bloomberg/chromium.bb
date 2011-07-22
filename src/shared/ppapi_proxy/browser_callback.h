// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_

#include "native_client/src/include/portability.h"

struct NaClSrpcChannel;
struct PP_CompletionCallback;

namespace ppapi_proxy {

// Returns a PP_CompletionCallback that will call the remote implementation of
// a callback by |callback_id| on the plugin side on |srpc_channel|.
// This callback allows for optimized synchronous completion.
// Allocates data that will be deleted by the underlying callback function.
// Returns NULL callback on failure.
struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id);
struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
     // For callbacks invoked on a byte read.
    int32_t bytes_to_read,
    char** buffer);

// If the callback won't be called, use this to clean up the data from
// the function above.
void DeleteRemoteCallbackInfo(struct PP_CompletionCallback callback);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_
