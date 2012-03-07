// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Gamepad functions.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_gamepad.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBGamepadInterface;

void PpbGamepadRpcServer::PPB_Gamepad_Sample(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Instance instance,
      // outputs
      nacl_abi_size_t* pads_bytes, char* pads) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (*pads_bytes != sizeof(struct PP_GamepadsSampleData))
    return;

  PPBGamepadInterface()->Sample(
      instance,
      reinterpret_cast<struct PP_GamepadsSampleData*>(pads));
  DebugPrintf("PPB_Gamepad::SampleGamepads\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}
