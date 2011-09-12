// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_url_loader.h"

#include <stdio.h>
#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_URLLoader::Create: instance=%"NACL_PRIu32"\n", instance);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_Create(
          GetMainSrpcChannel(), instance, &resource);
  DebugPrintf("PPB_URLLoader::Create: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  return kInvalidResourceId;
}

PP_Bool IsURLLoader(PP_Resource resource) {
  DebugPrintf("PPB_URLLoader::IsURLLoader: resource=%"NACL_PRIu32"\n",
              resource);

  int32_t is_url_loader;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_IsURLLoader(
          GetMainSrpcChannel(), resource, &is_url_loader);
  DebugPrintf("PPB_URLLoader::IsURLLoader: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_url_loader)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Open(PP_Resource loader,
             PP_Resource request,
             struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_URLLoader::Open: loader=%"NACL_PRIu32"\n", loader);
  DebugPrintf("PPB_URLLoader::Open: request=%"NACL_PRIu32"\n", request);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_Open(
          GetMainSrpcChannel(), loader, request, callback_id, &pp_error);
  DebugPrintf("PPB_URLLoader::Open: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t FollowRedirect(PP_Resource loader,
                       struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_URLLoader::FollowRedirect: loader=%"NACL_PRIu32"\n", loader);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_FollowRedirect(
          GetMainSrpcChannel(), loader, callback_id, &pp_error);
  DebugPrintf("PPB_URLLoader::FollowRedirect: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

PP_Bool GetUploadProgress(PP_Resource loader,
                          int64_t* bytes_sent,
                          int64_t* total_bytes_to_be_sent) {
  DebugPrintf("PPB_URLLoader::GetUploadProgress: loader=%"NACL_PRIu32"\n",
              loader);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_GetUploadProgress(
          GetMainSrpcChannel(),
          loader,
          bytes_sent,
          total_bytes_to_be_sent,
          &success);
 DebugPrintf("PPB_URLLoader::GetUploadProgress: %s\n",
             NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool GetDownloadProgress(PP_Resource loader,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  DebugPrintf("PPB_URLLoader::GetDownloadProgress: loader=%"NACL_PRIu32"\n",
              loader);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_GetDownloadProgress(
          GetMainSrpcChannel(),
          loader,
          bytes_received,
          total_bytes_to_be_received,
          &success);
 DebugPrintf("PPB_URLLoader::GetDownloadProgress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Resource GetResponseInfo(PP_Resource loader) {
  DebugPrintf("PPB_URLLoader::GetResponseInfo: loader=%"NACL_PRIu32"\n",
              loader);

  PP_Resource response;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_GetResponseInfo(
          GetMainSrpcChannel(),
          loader,
          &response);

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return response;
  return kInvalidResourceId;
}

int32_t ReadResponseBody(PP_Resource loader,
                         void* buffer,
                         int32_t bytes_to_read,
                         struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_URLLoader::ReadResponseBody: loader=%"NACL_PRIu32"\n",
              loader);
  if (bytes_to_read < 0)
    bytes_to_read = 0;
  nacl_abi_size_t buffer_size = bytes_to_read;

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback, buffer);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error_or_bytes;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_ReadResponseBody(
          GetMainSrpcChannel(),
          loader,
          bytes_to_read,
          callback_id,
          &buffer_size,
          static_cast<char*>(buffer),
          &pp_error_or_bytes);
  DebugPrintf("PPB_URLLoader::ReadResponseBody: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

int32_t FinishStreamingToFile(PP_Resource loader,
                              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_URLLoader::FinishStreamingToFile: loader=%"NACL_PRIu32"\n",
              loader);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_FinishStreamingToFile(
          GetMainSrpcChannel(), loader, callback_id, &pp_error);

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

void Close(PP_Resource loader) {
  DebugPrintf("PPB_URLLoader::Close: loader=%"NACL_PRIu32"\n", loader);

  NaClSrpcError srpc_result =
      PpbURLLoaderRpcClient::PPB_URLLoader_Close(
          GetMainSrpcChannel(), loader);
  DebugPrintf("PPB_URLLoader::Close: %s\n", NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_URLLoader* PluginURLLoader::GetInterface() {
  static const PPB_URLLoader url_loader_interface = {
    Create,
    IsURLLoader,
    Open,
    FollowRedirect,
    GetUploadProgress,
    GetDownloadProgress,
    GetResponseInfo,
    ReadResponseBody,
    FinishStreamingToFile,
    Close,
  };
  return &url_loader_interface;
}

}  // namespace ppapi_proxy
