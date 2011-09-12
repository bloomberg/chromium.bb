// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Instance functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "srpcgen/ppb_rpc.h"

void PpbInstanceRpcServer::PPB_Instance_BindGraphics(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource graphics_device,
    // outputs
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      ppapi_proxy::PPBInstanceInterface()->BindGraphics(
        instance,
        graphics_device);
  *success = (pp_success == PP_TRUE);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInstanceRpcServer::PPB_Instance_IsFullFrame(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    int32_t* is_full_frame) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_is_full_frame =
      ppapi_proxy::PPBInstanceInterface()->IsFullFrame(instance);
  *is_full_frame = (pp_is_full_frame == PP_TRUE);

  rpc->result = NACL_SRPC_RESULT_OK;
}
