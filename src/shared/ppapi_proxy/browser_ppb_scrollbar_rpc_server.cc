// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Scrollbar_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_image_data.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBScrollbarInterface;

void PpbScrollbarRpcServer::PPB_Scrollbar_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t vertical,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_Bool pp_vertical = vertical ? PP_TRUE : PP_FALSE;
  *resource = PPBScrollbarInterface()->Create(instance, pp_vertical);
  DebugPrintf("PPB_Scrollbar::Create: resource=%"NACL_PRIu32"\n", *resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_IsScrollbar(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* is_scrollbar) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool result = PPBScrollbarInterface()->IsScrollbar(resource);
  *is_scrollbar = (result == PP_TRUE);

  DebugPrintf(
      "PPB_Scrollbar::IsScrollbar: is_scrollbar=%"NACL_PRId32"\n",
      *is_scrollbar);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_GetThickness(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* thickness) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *thickness = PPBScrollbarInterface()->GetThickness(resource);

  DebugPrintf("PPB_Scrollbar::GetThickness: thickness=%"NACL_PRId32"\n",
              *thickness);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_IsOverlay(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* is_overlay) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool result = PPBScrollbarInterface()->IsOverlay(resource);
  *is_overlay = (result == PP_TRUE);

  DebugPrintf(
      "PPB_Scrollbar::IsScrollbar: is_overlay=%"NACL_PRId32"\n",
      *is_overlay);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_GetValue(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource scrollbar,
    int32_t* value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *value = PPBScrollbarInterface()->GetValue(scrollbar);

  DebugPrintf("PPB_Scrollbar::GetValue: value=%"NACL_PRId32"\n",
              *value);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_SetValue(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource scrollbar,
    int32_t value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBScrollbarInterface()->SetValue(scrollbar, value);

  DebugPrintf("PPB_Scrollbar::SetValue\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_SetDocumentSize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource scrollbar,
    int32_t size) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBScrollbarInterface()->SetDocumentSize(scrollbar, size);

  DebugPrintf("PPB_Scrollbar::SetDocumentSize\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_SetTickMarks(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource scrollbar,
    nacl_abi_size_t tick_marks_bytes, char* tick_marks,
    int32_t count) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (tick_marks_bytes != count * sizeof(struct PP_Rect))
    return;
  struct PP_Rect* pp_tick_marks =
      reinterpret_cast<struct PP_Rect*>(tick_marks);
  PPBScrollbarInterface()->SetTickMarks(scrollbar, pp_tick_marks, count);

  DebugPrintf("PPB_Scrollbar::SetTickMarks\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbScrollbarRpcServer::PPB_Scrollbar_ScrollBy(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource scrollbar,
    int32_t unit,
    int32_t multiplier) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_ScrollBy_Dev pp_unit = static_cast<PP_ScrollBy_Dev>(unit);
  PPBScrollbarInterface()->ScrollBy(scrollbar, pp_unit, multiplier);

  DebugPrintf("PPB_Scrollbar::ScrollBy\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

