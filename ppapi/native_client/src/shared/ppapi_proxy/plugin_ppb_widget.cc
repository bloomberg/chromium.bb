// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_widget.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_rect.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPRectBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Rect));

PP_Bool IsWidget(PP_Resource resource) {
  DebugPrintf("PPB_Widget::IsWidget: "
              "resource=%"NACL_PRIu32"\n", resource);

  int32_t is_widget = 0;
  NaClSrpcError srpc_result =
      PpbWidgetRpcClient::PPB_Widget_IsWidget(
          GetMainSrpcChannel(),
          resource,
          &is_widget);

  DebugPrintf("PPB_Widget::IsWidget: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_widget)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool Paint(
    PP_Resource widget,
    const struct PP_Rect* rect,
    PP_Resource image) {
  DebugPrintf("PPB_Widget::Paint: "
              "widget=%"NACL_PRIu32"\n", widget);

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbWidgetRpcClient::PPB_Widget_Paint(
          GetMainSrpcChannel(),
          widget,
          kPPRectBytes,
          reinterpret_cast<char*>(const_cast<struct PP_Rect*>(rect)),
          image,
          &success);

  DebugPrintf("PPB_Widget::Paint: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool HandleEvent(PP_Resource widget, PP_Resource event) {
  DebugPrintf("PPB_Widget::HandleEvent: widget=%"NACL_PRId32", "
              "event=%"NACL_PRId32"\n",
              widget, event);

  int32_t handled = 0;
  NaClSrpcError srpc_result =
      PpbWidgetRpcClient::PPB_Widget_HandleEvent(GetMainSrpcChannel(),
                                                 widget,
                                                 event,
                                                 &handled);

  DebugPrintf("PPB_Widget::HandleEvent: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && handled)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool GetLocation(
    PP_Resource widget,
    struct PP_Rect* location) {
  DebugPrintf("PPB_Widget::GetLocation: "
              "widget=%"NACL_PRIu32"\n", widget);

  int32_t visible = 0;
  nacl_abi_size_t location_size = kPPRectBytes;
  NaClSrpcError srpc_result =
      PpbWidgetRpcClient::PPB_Widget_GetLocation(
          GetMainSrpcChannel(),
          widget,
          &location_size,
          reinterpret_cast<char*>(location),
          &visible);

  DebugPrintf("PPB_Widget::GetLocation: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && visible &&
      location_size == kPPRectBytes)
    return PP_TRUE;
  return PP_FALSE;
}

void SetLocation(
    PP_Resource widget,
    const struct PP_Rect* location) {
  DebugPrintf("PPB_Widget::SetLocation: "
              "widget=%"NACL_PRIu32"\n", widget);

  NaClSrpcError srpc_result =
      PpbWidgetRpcClient::PPB_Widget_SetLocation(
          GetMainSrpcChannel(),
          widget,
          kPPRectBytes,
          reinterpret_cast<char*>(const_cast<struct PP_Rect*>(location)));

  DebugPrintf("PPB_Widget::SetLocation: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_Widget_Dev* PluginWidget::GetInterface() {
  static const PPB_Widget_Dev widget_interface = {
    IsWidget,
    Paint,
    HandleEvent,
    GetLocation,
    SetLocation
  };
  return &widget_interface;
}

}  // namespace ppapi_proxy

