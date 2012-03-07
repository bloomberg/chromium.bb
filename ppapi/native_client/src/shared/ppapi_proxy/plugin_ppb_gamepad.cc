// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_instance_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_gamepad.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_size.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

void SampleGamepads(PP_Instance instance,
                    struct PP_GamepadsSampleData* pads) {
  DebugPrintf("PPB_Gamepad::SampleGamepads: instance=%"NACL_PRId32"\n",
              instance);
  if (pads == NULL)
    return;

  nacl_abi_size_t pads_bytes =
      static_cast<nacl_abi_size_t>(sizeof(struct PP_GamepadsSampleData));
  NaClSrpcError srpc_result =
      PpbGamepadRpcClient::PPB_Gamepad_Sample(
          GetMainSrpcChannel(),
          instance,
          &pads_bytes,
          reinterpret_cast<char*>(pads));
  DebugPrintf("PPB_Gamepad::SampleGamepads: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pads->length = 0;
}

}  // namespace

const PPB_Gamepad* PluginGamepad::GetInterface() {
  static const PPB_Gamepad gamepad_interface = {
    SampleGamepads
  };
  return &gamepad_interface;
}

}  // namespace ppapi_proxy
