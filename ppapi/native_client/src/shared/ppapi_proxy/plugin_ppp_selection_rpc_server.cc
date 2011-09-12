// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Selection_Dev functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPSelectionInterface;
using ppapi_proxy::SerializeTo;

void PppSelectionRpcServer::PPP_Selection_GetSelectedText(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    int32_t html,
    // outputs
    nacl_abi_size_t* selected_text_bytes, char* selected_text) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PP_Bool pp_html = html ? PP_TRUE : PP_FALSE;
  PP_Var pp_selected_text =
      PPPSelectionInterface()->GetSelectedText(instance, pp_html);
  if (!SerializeTo(&pp_selected_text, selected_text, selected_text_bytes))
    return;

  rpc->result = NACL_SRPC_RESULT_OK;
}
