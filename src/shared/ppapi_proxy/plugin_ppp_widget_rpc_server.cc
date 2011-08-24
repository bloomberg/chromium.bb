// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Widget_Dev functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPWidgetInterface;

void PppWidgetRpcServer::PPP_Widget_Invalidate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource resource,
    nacl_abi_size_t dirty_rect_bytes, char* dirty_rect) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  if (dirty_rect_bytes != sizeof(struct PP_Rect))
    return;

  struct PP_Rect* pp_dirty_rect =
      reinterpret_cast<struct PP_Rect*>(dirty_rect);
  PPPWidgetInterface()->Invalidate(instance, resource, pp_dirty_rect);

  rpc->result = NACL_SRPC_RESULT_OK;
}
