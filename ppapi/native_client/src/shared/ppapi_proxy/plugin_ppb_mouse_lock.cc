// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_mouse_lock.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

int32_t LockMouse(PP_Instance instance, PP_CompletionCallback callback) {
  DebugPrintf("PPB_MouseLock::LockMouse: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbMouseLockRpcClient::PPB_MouseLock_LockMouse(
          GetMainSrpcChannel(),
          instance,
          callback_id,
          &pp_error);

  DebugPrintf("PPB_MouseLock::LockMouse: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

void UnlockMouse(PP_Instance instance) {
  DebugPrintf("PPB_MouseLock::UnlockMouse: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PpbMouseLockRpcClient::PPB_MouseLock_UnlockMouse(
          GetMainSrpcChannel(),
          instance);

  DebugPrintf("PPB_MouseLock::UnlockMouse: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_MouseLock_Dev* PluginMouseLock::GetInterface() {
  static const PPB_MouseLock_Dev mouse_lock_interface = {
    LockMouse,
    UnlockMouse
  };
  return &mouse_lock_interface;
}

}  // namespace ppapi_proxy


