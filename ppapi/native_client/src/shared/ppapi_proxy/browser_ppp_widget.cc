// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_widget.h"

// Include file order cannot be observed because ppp_instance declares a
// structure return type that causes an error on Windows.
// TODO(sehr, brettw): fix the return types and include order in PPAPI.
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPRectBytes =
    static_cast<nacl_abi_size_t>(sizeof(PP_Rect));

void Invalidate(PP_Instance instance,
                PP_Resource widget,
                const struct PP_Rect* dirty_rect) {
  DebugPrintf("PPP_Widget_Dev::Invalidate: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PppWidgetRpcClient::PPP_Widget_Invalidate(
          GetMainSrpcChannel(instance),
          instance,
          widget,
          kPPRectBytes,
          reinterpret_cast<char*>(const_cast<struct PP_Rect*>(dirty_rect)));

  DebugPrintf("PPP_Widget_Dev::Invalidate: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_Widget_Dev* BrowserWidget::GetInterface() {
  static const PPP_Widget_Dev widget_interface = {
    Invalidate
  };
  return &widget_interface;
}

}  // namespace ppapi_proxy

