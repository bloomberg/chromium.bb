// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Zoom functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPZoomInterface;

void PppZoomRpcServer::PPP_Zoom_Zoom(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    double factor,
    int32_t text_only) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PPPZoomInterface()->Zoom(instance, factor, text_only ? PP_TRUE : PP_FALSE);

  DebugPrintf("PPP_Zoom::Zoom");
  rpc->result = NACL_SRPC_RESULT_OK;
}
