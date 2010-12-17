/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_url_request_info.h"

#include <stdio.h>
#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Module module) {
  DebugPrintf("Plugin::PPB_UrlRequestInfo::Create: module=%"
              NACL_PRIx64"\n", module);
  NACL_UNTESTED();

  int64_t resource;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_Create(
          GetMainSrpcChannel(),
          static_cast<int64_t>(module),
          &resource);

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return static_cast<PP_Resource>(resource);
  return kInvalidResourceId;
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  DebugPrintf("Plugin::PPB_UrlRequestInfo::IsURLRequestInfo: resource=%"
              NACL_PRIx64"\n", resource);
  NACL_UNTESTED();

  int32_t is_url_request_info;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_IsURLRequestInfo(
          GetMainSrpcChannel(),
          static_cast<int64_t>(resource),
          &is_url_request_info);

  if (srpc_result == NACL_SRPC_RESULT_OK && is_url_request_info)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool SetProperty(PP_Resource request,
                    PP_URLRequestProperty property,
                    struct PP_Var value) {
  DebugPrintf("Plugin::PPB_UrlRequestInfo::SetProperty: request=%"
              NACL_PRIx64"\n", request);
  NACL_UNTESTED();

  nacl_abi_size_t value_size = kMaxVarSize;
  nacl::scoped_array<char> value_bytes(Serialize(&value, 1, &value_size));

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_SetProperty(
          GetMainSrpcChannel(),
          static_cast<int64_t>(request),
          static_cast<int32_t>(property),
          value_size,
          value_bytes.get(),
          &success);
  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool AppendDataToBody(PP_Resource request, const char* data, uint32_t len) {
  DebugPrintf("Plugin::PPB_UrlRequestInfo::AppendDataToBody: request=%"
              NACL_PRIx64"\n", request);
  NACL_UNTESTED();

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendDataToBody(
          GetMainSrpcChannel(),
          static_cast<int64_t>(request),
          static_cast<nacl_abi_size_t>(len),
          const_cast<char*>(data),
          &success);

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool AppendFileToBody(PP_Resource request,
                         PP_Resource file_ref,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  DebugPrintf("Plugin::PPB_UrlRequestInfo::AppendFileToBody: request=%"
              NACL_PRIx64"\n", request);
  NACL_UNTESTED();

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendFileToBody(
          GetMainSrpcChannel(),
          static_cast<int64_t>(request),
          static_cast<int64_t>(file_ref),
          start_offset,
          number_of_bytes,
          static_cast<double>(expected_last_modified_time),
          &success);

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

}  // namespace

const PPB_URLRequestInfo* PluginURLRequestInfo::GetInterface() {
  static const PPB_URLRequestInfo url_request_info_interface = {
    Create,
    IsURLRequestInfo,
    SetProperty,
    AppendDataToBody,
    AppendFileToBody,
  };
  return &url_request_info_interface;
}

}  // namespace ppapi_proxy
