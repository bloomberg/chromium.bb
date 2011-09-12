// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_cursor_control.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPPointBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Point));

PP_Bool SetCursor(PP_Instance instance,
                  enum PP_CursorType_Dev type,
                  PP_Resource custom_image,
                  const struct PP_Point* hot_spot) {
  DebugPrintf("PPB_CursorControl::SetCursor: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t hot_spot_size = (hot_spot != NULL) ? kPPPointBytes : 0;
  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbCursorControlRpcClient::PPB_CursorControl_SetCursor(
          GetMainSrpcChannel(),
          instance,
          type,
          custom_image,
          hot_spot_size,
          reinterpret_cast<char*>(const_cast<struct PP_Point*>(hot_spot)),
          &success);

  DebugPrintf("PPB_CursorControl::SetCursor: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool LockCursor(PP_Instance instance) {
  DebugPrintf("PPB_CursorControl::LockCursor: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbCursorControlRpcClient::PPB_CursorControl_LockCursor(
          GetMainSrpcChannel(),
          instance,
          &success);

  DebugPrintf("PPB_CursorControl::LockCursor: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool UnlockCursor(PP_Instance instance) {
  DebugPrintf("PPB_CursorControl::UnlockCursor: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbCursorControlRpcClient::PPB_CursorControl_UnlockCursor(
          GetMainSrpcChannel(),
          instance,
          &success);

  DebugPrintf("PPB_CursorControl::UnlockCursor: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool HasCursorLock(PP_Instance instance) {
  DebugPrintf("PPB_CursorControl::HasCursorLock: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbCursorControlRpcClient::PPB_CursorControl_HasCursorLock(
          GetMainSrpcChannel(),
          instance,
          &success);

  DebugPrintf("PPB_CursorControl::HasCursorLock: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool CanLockCursor(PP_Instance instance) {
  DebugPrintf("PPB_CursorControl::CanLockCursor: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbCursorControlRpcClient::PPB_CursorControl_CanLockCursor(
          GetMainSrpcChannel(),
          instance,
          &success);

  DebugPrintf("PPB_CursorControl::CanLockCursor: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

}  // namespace

const PPB_CursorControl_Dev* PluginCursorControl::GetInterface() {
  static const PPB_CursorControl_Dev cursor_control_interface = {
    SetCursor,
    LockCursor,
    UnlockCursor,
    HasCursorLock,
    CanLockCursor
  };
  return &cursor_control_interface;
}

}  // namespace ppapi_proxy


