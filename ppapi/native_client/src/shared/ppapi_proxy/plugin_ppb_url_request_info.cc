// Copyright 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

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

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_URLRequestInfo::Create: instance=%"NACL_PRIu32"\n",
              instance);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_Create(
          GetMainSrpcChannel(), instance, &resource);
  DebugPrintf("PPB_URLRequestInfo::Create: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  return kInvalidResourceId;
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  DebugPrintf("PPB_URLRequestInfo::IsURLRequestInfo: resource=%"NACL_PRIu32"\n",
              resource);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_IsURLRequestInfo(
          GetMainSrpcChannel(), resource, &success);
  DebugPrintf("PPB_URLRequestInfo::IsURLRequestInfo: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool SetProperty(PP_Resource request,
                    PP_URLRequestProperty property,
                    struct PP_Var value) {
  DebugPrintf("PPB_URLRequestInfo::SetProperty: request=%"NACL_PRIu32"\n",
              request);

  nacl_abi_size_t value_size = kMaxVarSize;
  nacl::scoped_array<char> value_bytes(Serialize(&value, 1, &value_size));

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_SetProperty(
          GetMainSrpcChannel(),
          request,
          static_cast<int32_t>(property),
          value_size,
          value_bytes.get(),
          &success);
  DebugPrintf("PPB_URLRequestInfo::SetProperty: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool AppendDataToBody(PP_Resource request, const void* data, uint32_t len) {
  DebugPrintf("PPB_URLRequestInfo::AppendDataToBody: request=%"NACL_PRIu32"\n",
              request);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendDataToBody(
          GetMainSrpcChannel(),
          request,
          static_cast<nacl_abi_size_t>(len),
          static_cast<char*>(const_cast<void*>(data)),
          &success);
  DebugPrintf("PPB_URLRequestInfo::AppendDataToBody: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool AppendFileToBody(PP_Resource request,
                         PP_Resource file_ref,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  DebugPrintf("PPB_URLRequestInfo::AppendFileToBody: request=%"NACL_PRIu32"\n",
              request);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendFileToBody(
          GetMainSrpcChannel(),
          request,
          file_ref,
          start_offset,
          number_of_bytes,
          static_cast<double>(expected_last_modified_time),
          &success);
  DebugPrintf("PPB_URLRequestInfo::AppendFileToBody: %s\n",
              NaClSrpcErrorString(srpc_result));

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
