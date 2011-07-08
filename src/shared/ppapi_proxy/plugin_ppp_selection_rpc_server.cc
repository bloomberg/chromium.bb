// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Selection_Dev functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_selection_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_resource.h"
#include "native_client/src/third_party/ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::SerializeTo;

namespace {

const PPP_Selection_Dev* PPPSelection() {
  static const PPP_Selection_Dev* ppp_selection = NULL;
  if (ppp_selection == NULL) {
    ppp_selection = reinterpret_cast<const PPP_Selection_Dev*>(
        ::PPP_GetInterface(PPP_SELECTION_DEV_INTERFACE));
  }
  return ppp_selection;
}

}  // namespace

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

  const PPP_Selection_Dev* ppp_selection = PPPSelection();
  if (ppp_selection == NULL || ppp_selection->GetSelectedText == NULL)
    return;
  PP_Bool pp_html = html ? PP_TRUE : PP_FALSE;
  PP_Var pp_selected_text = ppp_selection->GetSelectedText(instance, pp_html);
  if (!SerializeTo(&pp_selected_text, selected_text, selected_text_bytes))
    return;

  rpc->result = NACL_SRPC_RESULT_OK;
}

