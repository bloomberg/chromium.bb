// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"

struct NaClSrpcChannel;
struct PP_CompletionCallback;

namespace ppapi_proxy {

// Pointer to function to evaluate the result of a read operation.
typedef bool (*CheckResultFunc)(int32_t result);
// Pointer to function to retrieve/calculate the size read.
typedef nacl_abi_size_t (*GetReadSizeFunc)(int32_t result);

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
struct PP_CompletionCallback MakeRemoteCompletionCallback(
    NaClSrpcChannel* srpc_channel,
    int32_t callback_id,
     // For callbacks invoked on a byte read.
    int32_t bytes_to_read,
    char** buffer,
    CheckResultFunc check_result,
    GetReadSizeFunc get_size_read_func);

// If the callback won't be called, use this to clean up the data from
// the function above.
void DeleteRemoteCallbackInfo(struct PP_CompletionCallback callback);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_CALLBACK_H_
