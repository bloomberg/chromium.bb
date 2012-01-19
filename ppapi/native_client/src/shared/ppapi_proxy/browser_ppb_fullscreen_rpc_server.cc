// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Fullscreen functions.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBFullscreenInterface;

// IsFullscreen is implemented via an extra flag to the DidChangeView proxy.

void PpbFullscreenRpcServer::PPB_Fullscreen_SetFullscreen(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Instance instance,
      int32_t fullscreen,
      // outputs
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBFullscreenInterface()->SetFullscreen(
      instance,
      PP_FromBool(fullscreen));
  *success = PP_ToBool(pp_success);
  DebugPrintf("PPB_Fullscreen::SetFullscreen: success=%d\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}


void PpbFullscreenRpcServer::PPB_Fullscreen_GetScreenSize(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Instance instance,
      // outputs
      nacl_abi_size_t* size_bytes, char* size,
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = 0;
  if (*size_bytes != sizeof(struct PP_Size))
    return;

  PP_Bool pp_success = PPBFullscreenInterface()->GetScreenSize(
      instance,
      reinterpret_cast<struct PP_Size*>(size));
  *success = PP_ToBool(pp_success);
  DebugPrintf("PPB_Fullscreen::GetScreenSize: success=%d\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}
