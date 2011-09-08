// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Fullscreen_Dev functions.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/pp_size.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBFullscreenInterface;

void PpbFullscreenRpcServer::PPB_Fullscreen_IsFullscreen(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Instance instance,
      // outputs
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBFullscreenInterface()->IsFullscreen(instance);
  DebugPrintf("PPB_Fullscreen::IsFullscreen: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}


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
      (fullscreen ? PP_TRUE : PP_FALSE));
  DebugPrintf("PPB_Fullscreen::SetFullscreen: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
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
  DebugPrintf("PPB_Fullscreen::GetScreenSize: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}
