// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Zoom_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/ppb_image_data.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBZoomInterface;

void PpbZoomRpcServer::PPB_Zoom_ZoomChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    double factor) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBZoomInterface()->ZoomChanged(instance, factor);

  DebugPrintf("PPB_Zoom::ZoomChanged: instance=%"NACL_PRIu32"\n",
              instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbZoomRpcServer::PPB_Zoom_ZoomLimitsChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    double minimum_factor,
    double maximum_factor) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBZoomInterface()->ZoomLimitsChanged(instance,
                                        minimum_factor,
                                        maximum_factor);

  DebugPrintf("PPB_Zoom::ZoomLimitsChanged: instance=%"NACL_PRIu32"\n",
              instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

