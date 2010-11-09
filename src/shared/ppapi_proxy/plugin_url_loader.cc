/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_url_loader.h"

#include <stdio.h>
#include <string.h>
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/dev/ppb_url_loader_dev.h"

namespace ppapi_proxy {

namespace {
PP_Resource Create(PP_Instance instance) {
  UNREFERENCED_PARAMETER(instance);
  return kInvalidResourceId;
}

PP_Bool IsURLLoader(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return PP_FALSE;
}

int32_t Open(PP_Resource loader,
             PP_Resource request_info,
             struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(loader);
  UNREFERENCED_PARAMETER(request_info);
  UNREFERENCED_PARAMETER(callback);
  return PP_ERROR_BADRESOURCE;
}

int32_t FollowRedirect(PP_Resource loader,
                       struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(loader);
  UNREFERENCED_PARAMETER(callback);
  return PP_ERROR_BADRESOURCE;
}

PP_Bool GetUploadProgress(PP_Resource loader,
                       int64_t* bytes_sent,
                       int64_t* total_bytes_to_be_sent) {
  UNREFERENCED_PARAMETER(loader);
  UNREFERENCED_PARAMETER(bytes_sent);
  UNREFERENCED_PARAMETER(total_bytes_to_be_sent);
  return PP_FALSE;
}

PP_Bool GetDownloadProgress(PP_Resource loader,
                         int64_t* bytes_received,
                         int64_t* total_bytes_to_be_received) {
  UNREFERENCED_PARAMETER(loader);
  UNREFERENCED_PARAMETER(bytes_received);
  UNREFERENCED_PARAMETER(total_bytes_to_be_received);
  return PP_FALSE;
}

PP_Resource GetResponseInfo(PP_Resource loader) {
  UNREFERENCED_PARAMETER(loader);
  return kInvalidResourceId;
}

int32_t ReadResponseBody(PP_Resource loader,
                         char* buffer,
                         int32_t bytes_to_read,
                         struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(loader);
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(bytes_to_read);
  UNREFERENCED_PARAMETER(callback);
  return PP_ERROR_BADRESOURCE;
}

int32_t FinishStreamingToFile(PP_Resource loader,
                              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(loader);
  UNREFERENCED_PARAMETER(callback);
  return PP_ERROR_BADRESOURCE;
}

void Close(PP_Resource loader) {
  UNREFERENCED_PARAMETER(loader);
}
}  // namespace

const PPB_URLLoader_Dev* PluginURLLoader::GetInterface() {
  static const PPB_URLLoader_Dev intf = {
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
  return &intf;
}

}  // namespace ppapi_proxy
