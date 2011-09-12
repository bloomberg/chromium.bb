// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_URLLoader functions.

#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::PPBURLLoaderInterface;
using ppapi_proxy::DebugPrintf;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::DeleteRemoteCallbackInfo;

void PpbURLLoaderRpcServer::PPB_URLLoader_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputsles
    PP_Instance instance,
    // outputs
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource = PPBURLLoaderInterface()->Create(instance);
  DebugPrintf("PPB_URLLoader::Create: resource=%"NACL_PRIu32"\n", *resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_IsURLLoader(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // outputs
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBURLLoaderInterface()->IsURLLoader(resource);
  DebugPrintf("PPB_URLLoader::IsURLLoader: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_Open(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    PP_Resource request,
    int32_t callback_id,
    // outputs
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBURLLoaderInterface()->Open(loader, request, remote_callback);
  DebugPrintf("PPB_URLLoader::Open: pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_FollowRedirect(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    int32_t callback_id,
    // outputs
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBURLLoaderInterface()->FollowRedirect(loader, remote_callback);
  DebugPrintf("PPB_URLLoader::FollowRedirect: pp_error=%"NACL_PRId32"\n",
              *pp_error);
  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_GetUploadProgress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    // outputs
    int64_t* bytes_sent,
    int64_t* total_bytes_to_be_sent,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBURLLoaderInterface()->GetUploadProgress(
      loader, bytes_sent, total_bytes_to_be_sent);
  DebugPrintf("PPB_URLLoader::GetUploadProgress: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_GetDownloadProgress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    // outputs
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBURLLoaderInterface()->GetDownloadProgress(
      loader, bytes_received, total_bytes_to_be_received);
  DebugPrintf("PPB_URLLoader::GetDownloadProgress: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_GetResponseInfo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    // outputs
    PP_Resource* response) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *response = PPBURLLoaderInterface()->GetResponseInfo(loader);
  DebugPrintf("PPB_URLLoader::GetResponseInfo: response=%"NACL_PRIu32"\n",
              *response);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_ReadResponseBody(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    int32_t bytes_to_read,
    int32_t callback_id,
    // outputs
    nacl_abi_size_t* buffer_size, char* buffer,
    int32_t* pp_error_or_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  CHECK(*buffer_size == static_cast<nacl_abi_size_t>(bytes_to_read));

  // |buffer| is allocated by the rpc server and will be freed after this
  // function returns. Hence we must provide our own byte storage for
  // a non-blocking call and tie it to the callback.
  char* callback_buffer = NULL;
  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id, bytes_to_read, &callback_buffer);
  if (remote_callback.func == NULL)
    return;

  *pp_error_or_bytes = PPBURLLoaderInterface()->ReadResponseBody(
      loader, callback_buffer, bytes_to_read, remote_callback);
  DebugPrintf("PPB_URLLoader::ReadResponseBody: pp_error_or_bytes=%"
              NACL_PRId32"\n", *pp_error_or_bytes);
  CHECK(*pp_error_or_bytes <= bytes_to_read);

  if (*pp_error_or_bytes > 0) {  // Bytes read into |callback_buffer|.
    // No callback scheduled.
    *buffer_size = static_cast<nacl_abi_size_t>(*pp_error_or_bytes);
    memcpy(buffer, callback_buffer, *buffer_size);
    DeleteRemoteCallbackInfo(remote_callback);
  } else if (*pp_error_or_bytes != PP_OK_COMPLETIONPENDING) {  // Async error.
    // No callback scheduled.
    *buffer_size = 0;
    DeleteRemoteCallbackInfo(remote_callback);
  } else {
    // Callback scheduled.
    *buffer_size = 0;
   }

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_FinishStreamingToFile(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader,
    int32_t callback_id,
    // outputs
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error =
      PPBURLLoaderInterface()->FinishStreamingToFile(loader, remote_callback);
  DebugPrintf("PPB_URLLoader::FinishStreamingToFile: pp_error=%"NACL_PRId32"\n",
              *pp_error);
  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLLoaderRpcServer::PPB_URLLoader_Close(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource loader) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PPBURLLoaderInterface()->Close(loader);
  rpc->result = NACL_SRPC_RESULT_OK;
}
