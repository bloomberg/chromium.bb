// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_zoom.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

void ZoomChanged(PP_Instance instance,
                    double factor) {
  DebugPrintf("PPB_Zoom::ZoomChanged: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PpbZoomRpcClient::PPB_Zoom_ZoomChanged(
          GetMainSrpcChannel(),
          instance,
          factor);

  DebugPrintf("PPB_Zoom::ZoomChanged: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void ZoomLimitsChanged(PP_Instance instance,
                       double minimum_factor,
                       double maximum_factor) {
  DebugPrintf("PPB_Zoom::ZoomLimitsChanged: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PpbZoomRpcClient::PPB_Zoom_ZoomLimitsChanged(
          GetMainSrpcChannel(),
          instance,
          minimum_factor,
          maximum_factor);

  DebugPrintf("PPB_Zoom::ZoomLimitsChanged: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_Zoom_Dev* PluginZoom::GetInterface() {
  static const PPB_Zoom_Dev zoom_interface = {
    ZoomChanged,
    ZoomLimitsChanged
  };
  return &zoom_interface;
}

}  // namespace ppapi_proxy


