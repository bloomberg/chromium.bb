// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_scrollbar.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPRectBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Rect));

PP_Resource Create(PP_Instance instance, PP_Bool vertical) {
  DebugPrintf("PPB_Scrollbar::Create: "
              "instance=%"NACL_PRIu32"\n",
              instance);

  PP_Resource scrollbar = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_Create(
          GetMainSrpcChannel(),
          instance,
          vertical,
          &scrollbar);

  DebugPrintf("PPB_Scrollbar::Create: %s\n",
              NaClSrpcErrorString(srpc_result));
  return scrollbar;
}

PP_Bool IsScrollbar(PP_Resource resource) {
  DebugPrintf("PPB_Scrollbar::IsScrollbar: "
              "resource=%"NACL_PRIu32"\n",
              resource);

  int32_t is_scrollbar = 0;
  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_IsScrollbar(
          GetMainSrpcChannel(),
          resource,
          &is_scrollbar);

  DebugPrintf("PPB_Scrollbar::IsScrollbar: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_scrollbar)
    return PP_TRUE;
  return PP_FALSE;
}

uint32_t GetThickness(PP_Resource resource) {
  DebugPrintf("PPB_Scrollbar::GetThickness");

  int32_t thickness = 0;
  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_GetThickness(
          GetMainSrpcChannel(),
          resource,
          &thickness);

  DebugPrintf("PPB_Scrollbar::GetThickness: %s\n",
              NaClSrpcErrorString(srpc_result));

  return thickness;
}

PP_Bool IsOverlay(PP_Resource resource) {
  DebugPrintf("PPB_Scrollbar::IsOverlay: "
              "resource=%"NACL_PRIu32"\n",
              resource);

  int32_t is_overlay = 0;
  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_IsOverlay(
          GetMainSrpcChannel(),
          resource,
          &is_overlay);

  DebugPrintf("PPB_Scrollbar::IsOverlay: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_overlay)
    return PP_TRUE;
  return PP_FALSE;
}

uint32_t GetValue(PP_Resource scrollbar) {
  DebugPrintf("PPB_Scrollbar::GetValue: "
              "scrollbar=%"NACL_PRIu32"\n",
              scrollbar);

  int32_t value = 0;
  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_GetValue(
          GetMainSrpcChannel(),
          scrollbar,
          &value);

  DebugPrintf("PPB_Scrollbar::GetValue: %s\n",
              NaClSrpcErrorString(srpc_result));

  return value;
}

void SetValue(PP_Resource scrollbar, uint32_t value) {
  DebugPrintf("PPB_Scrollbar::SetValue: "
              "scrollbar=%"NACL_PRIu32"\n",
              scrollbar);

  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_SetValue(
          GetMainSrpcChannel(),
          scrollbar,
          value);

  DebugPrintf("PPB_Scrollbar::SetValue: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void SetDocumentSize(PP_Resource scrollbar, uint32_t size) {
  DebugPrintf("PPB_Scrollbar::SetDocumentSize: "
              "scrollbar=%"NACL_PRIu32"\n", scrollbar);

  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_SetDocumentSize(
          GetMainSrpcChannel(),
          scrollbar,
          size);

  DebugPrintf("PPB_Scrollbar::SetDocumentSize: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void SetTickMarks(PP_Resource scrollbar,
                  const struct PP_Rect* tick_marks,
                  uint32_t count) {
  DebugPrintf("PPB_Scrollbar::SetTickMarks: "
              "scrollbar=%"NACL_PRIu32"\n",
              scrollbar);

  nacl_abi_size_t tick_marks_bytes = kPPRectBytes * count;
  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_SetTickMarks(
          GetMainSrpcChannel(),
          scrollbar,
          tick_marks_bytes,
          reinterpret_cast<char*>(const_cast<struct PP_Rect*>(tick_marks)),
          count);

  DebugPrintf("PPB_Scrollbar::SetTickMarks: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void ScrollBy(PP_Resource scrollbar,
              PP_ScrollBy_Dev unit,
              int32_t multiplier) {
  DebugPrintf("PPB_Scrollbar::ScrollBy: "
              "scrollbar=%"NACL_PRIu32"\n",
              scrollbar);

  NaClSrpcError srpc_result =
      PpbScrollbarRpcClient::PPB_Scrollbar_ScrollBy(
          GetMainSrpcChannel(),
          scrollbar,
          unit,
          multiplier);

  DebugPrintf("PPB_Scrollbar::ScrollBy: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_Scrollbar_Dev* PluginScrollbar::GetInterface() {
  static const PPB_Scrollbar_Dev scrollbar_interface = {
    Create,
    IsScrollbar,
    GetThickness,
    IsOverlay,
    GetValue,
    SetValue,
    SetDocumentSize,
    SetTickMarks,
    ScrollBy
  };
  return &scrollbar_interface;
}

}  // namespace ppapi_proxy

