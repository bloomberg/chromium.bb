// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Zoom functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_zoom_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_resource.h"
#include "native_client/src/third_party/ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;

namespace {

const PPP_Zoom_Dev* PPPZoom() {
  static const PPP_Zoom_Dev* ppp_zoom = NULL;
  if (ppp_zoom == NULL) {
    ppp_zoom = reinterpret_cast<const PPP_Zoom_Dev*>(
        ::PPP_GetInterface(PPP_ZOOM_DEV_INTERFACE));
  }
  return ppp_zoom;
}

} // namespace

void PppZoomRpcServer::PPP_Zoom_Zoom(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    double factor,
    int32_t text_only) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  const PPP_Zoom_Dev* ppp_zoom = PPPZoom();
  if (ppp_zoom == NULL || ppp_zoom->Zoom == NULL)
    return;
  ppp_zoom->Zoom(instance, factor, text_only ? PP_TRUE : PP_FALSE);

  DebugPrintf("PPP_Zoom::Zoom");
  rpc->result = NACL_SRPC_RESULT_OK;
}

