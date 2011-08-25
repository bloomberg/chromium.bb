// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_url_response_info.h"

#include <stdio.h>
#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource) {
  DebugPrintf("PPB_URLResponseInfo::IsURLResponseInfo: resource=%"NACL_PRIu32
              "\n", resource);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_IsURLResponseInfo(
          GetMainSrpcChannel(), resource, &success);
  DebugPrintf("PPB_URLResponseInfo::IsURLResponseInfo: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Var GetProperty(PP_Resource response, PP_URLResponseProperty property) {
  DebugPrintf("PPB_URLResponseInfo::GetProperty: response=%"NACL_PRIu32"\n",
              response);
  NaClSrpcChannel* channel = GetMainSrpcChannel();

  PP_Var value = PP_MakeUndefined();
  nacl_abi_size_t value_size = kMaxVarSize;
  nacl::scoped_array<char> value_bytes(new char[kMaxVarSize]);

  NaClSrpcError srpc_result =
      PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_GetProperty(
          channel,
          response,
          static_cast<int32_t>(property),
          &value_size,
          value_bytes.get());
  DebugPrintf("PPB_URLResponseInfo::GetProperty: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    (void) DeserializeTo(channel, value_bytes.get(), value_size, 1, &value);
  return value;
}

PP_Resource GetBodyAsFileRef(PP_Resource response) {
  DebugPrintf("PPB_URLResponseInfo::GetBodyAsFileRef: response=%"NACL_PRIu32
              "\n", response);

  PP_Resource file_ref;
  NaClSrpcError srpc_result =
      PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_GetBodyAsFileRef(
          GetMainSrpcChannel(), response, &file_ref);
  DebugPrintf("PPB_URLResponseInfo::GetBodyAsFileRef: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return file_ref;
  return kInvalidResourceId;
}

}  // namespace

const PPB_URLResponseInfo* PluginURLResponseInfo::GetInterface() {
  static const PPB_URLResponseInfo url_response_info_interface = {
    IsURLResponseInfo,
    GetProperty,
    GetBodyAsFileRef,
  };
  return &url_response_info_interface;
}

}  // namespace ppapi_proxy
