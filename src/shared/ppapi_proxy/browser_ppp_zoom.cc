// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_zoom.h"

// Include file order cannot be observed because ppp_instance declares a
// structure return type that causes an error on Windows.
// TODO(sehr, brettw): fix the return types and include order in PPAPI.
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

namespace ppapi_proxy {

namespace {

void Zoom(PP_Instance instance,
          double factor,
          PP_Bool text_only) {
  DebugPrintf("PPP_Zoom::Zoom: instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result = PppZoomRpcClient::PPP_Zoom_Zoom(
      GetMainSrpcChannel(instance),
      instance,
      factor,
      static_cast<int32_t>(text_only));

  DebugPrintf("PPP_Zoom::Zoom: %s\n", NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_Zoom_Dev* BrowserZoom::GetInterface() {
  static const PPP_Zoom_Dev zoom_interface = {
    Zoom
  };
  return &zoom_interface;
}

}  // namespace ppapi_proxy

