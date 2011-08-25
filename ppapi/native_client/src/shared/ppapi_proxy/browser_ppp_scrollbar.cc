// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_scrollbar.h"

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

void ValueChanged(
    PP_Instance instance,
    PP_Resource resource,
    uint32_t value) {
  DebugPrintf("PPP_Scrollbar_Dev::ValueChanged: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result = PppScrollbarRpcClient::PPP_Scrollbar_ValueChanged(
      GetMainSrpcChannel(instance),
      instance,
      resource,
      value);

  DebugPrintf("PPP_Scrollbar_Dev::ValueChanged: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void OverlayChanged(
    PP_Instance instance,
    PP_Resource resource,
    PP_Bool type) {
  DebugPrintf("PPP_Scrollbar_Dev::OverlayChanged: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PppScrollbarRpcClient::PPP_Scrollbar_OverlayChanged(
          GetMainSrpcChannel(instance),
          instance,
          resource,
          type);

  DebugPrintf("PPP_Scrollbar_Dev::OverlayChanged: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_Scrollbar_Dev* BrowserScrollbar::GetInterface() {
  static const PPP_Scrollbar_Dev scrollbar_interface = {
    ValueChanged,
    OverlayChanged
  };
  return &scrollbar_interface;
}

}  // namespace ppapi_proxy

