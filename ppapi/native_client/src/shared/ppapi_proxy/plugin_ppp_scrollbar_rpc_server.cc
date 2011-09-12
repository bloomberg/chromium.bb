// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Scrollbar functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPScrollbarInterface;

void PppScrollbarRpcServer::PPP_Scrollbar_ValueChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource resource,
    int32_t value) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PPPScrollbarInterface()->ValueChanged(instance, resource, value);

  DebugPrintf("PPP_Scrollbar::ValueChanged: value=%"NACL_PRId32"\n", value);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppScrollbarRpcServer::PPP_Scrollbar_OverlayChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource resource,
    int32_t overlay) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PPPScrollbarInterface()->OverlayChanged(
      instance, resource, overlay ? PP_TRUE : PP_FALSE);

  DebugPrintf("PPP_Scrollbar::OverlayChanged: value=%s\n",
              overlay ? "true" : "false");
  rpc->result = NACL_SRPC_RESULT_OK;
}
