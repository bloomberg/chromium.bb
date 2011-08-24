// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Widget_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_image_data.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBWidgetInterface;

void PpbWidgetRpcServer::PPB_Widget_IsWidget(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* is_widget) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_is_widget = PPBWidgetInterface()->IsWidget(resource);
  *is_widget = (pp_is_widget == PP_TRUE);

  DebugPrintf("PPB_Widget::Widget: resource=%"NACL_PRIu32"\n",
              resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWidgetRpcServer::PPB_Widget_Paint(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource widget,
    nacl_abi_size_t rect_size, char* rect,
    PP_Resource image,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (rect_size != sizeof(struct PP_Rect))
    return;
  struct PP_Rect* pp_rect = reinterpret_cast<struct PP_Rect*>(rect);
  *success = PPBWidgetInterface()->Paint(widget, pp_rect, image);

  DebugPrintf("PPB_Widget::Paint: widget=%"NACL_PRIu32"\n",
              widget);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWidgetRpcServer::PPB_Widget_HandleEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource widget,
    PP_Resource pp_event,
    int32_t* handled) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *handled = PPBWidgetInterface()->HandleEvent(widget, pp_event);

  DebugPrintf("PPB_Widget::HandleEvent: widget=%"NACL_PRIu32"\n",
              widget);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWidgetRpcServer::PPB_Widget_GetLocation(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource widget,
    nacl_abi_size_t* rect_size, char* rect,
    int32_t* visible) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*rect_size != sizeof(struct PP_Rect))
    return;
  struct PP_Rect* pp_rect = reinterpret_cast<struct PP_Rect*>(rect);
  *visible = PPBWidgetInterface()->GetLocation(widget, pp_rect);

  DebugPrintf("PPB_Widget::GetLocation: widget=%"NACL_PRIu32"\n",
              widget);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWidgetRpcServer::PPB_Widget_SetLocation(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource widget,
    nacl_abi_size_t rect_size, char* rect) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (rect_size != sizeof(struct PP_Rect))
    return;
  struct PP_Rect* pp_rect = reinterpret_cast<struct PP_Rect*>(rect);
  PPBWidgetInterface()->SetLocation(widget, pp_rect);

  DebugPrintf("PPB_Widget::SetLocation: widget=%"NACL_PRIu32"\n",
              widget);
  rpc->result = NACL_SRPC_RESULT_OK;
}

