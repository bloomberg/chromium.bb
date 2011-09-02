// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_fullscreen.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/pp_size.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Bool IsFullscreen(PP_Instance instance) {
  DebugPrintf("PPB_Fullscreen::IsFullscreen: instance=%"NACL_PRIu32"\n",
              instance);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbFullscreenRpcClient::PPB_Fullscreen_IsFullscreen(
          GetMainSrpcChannel(), instance, &success);
  DebugPrintf("PPB_Fullscreen::IsFullscreen: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}


PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) {
  DebugPrintf("PPB_Fullscreen::SetFullscreen: "
              "instance=%"NACL_PRIu32" fullscreen=%d\n", instance, fullscreen);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbFullscreenRpcClient::PPB_Fullscreen_SetFullscreen(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(fullscreen),
          &success);
  DebugPrintf("PPB_Fullscreen::SetFullscreen: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}


PP_Bool GetScreenSize(PP_Instance instance, struct PP_Size* size) {
  DebugPrintf("PPB_Fullscreen::GetScreenSize: instance=%"NACL_PRIu32"\n",
              instance);
  if (size == NULL)
    return PP_FALSE;

  int32_t success;
  nacl_abi_size_t size_bytes =
      static_cast<nacl_abi_size_t>(sizeof(struct PP_Size));
  NaClSrpcError srpc_result =
      PpbFullscreenRpcClient::PPB_Fullscreen_GetScreenSize(
          GetMainSrpcChannel(),
          instance,
          &size_bytes,
          reinterpret_cast<char*>(size),
          &success);
  DebugPrintf("PPB_Fullscreen::GetScreenSize: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

}  // namespace

const PPB_Fullscreen_Dev* PluginFullscreen::GetInterface() {
  static const PPB_Fullscreen_Dev fullscreen_interface = {
    IsFullscreen,
    SetFullscreen,
    GetScreenSize
  };
  return &fullscreen_interface;
}

}  // namespace ppapi_proxy
