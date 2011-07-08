// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Scrollbar functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_scrollbar_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_resource.h"
#include "native_client/src/third_party/ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;

namespace {

const PPP_Scrollbar_Dev* PPPScrollbar() {
  static const PPP_Scrollbar_Dev* ppp_scrollbar = NULL;
  if (ppp_scrollbar == NULL) {
    ppp_scrollbar = reinterpret_cast<const PPP_Scrollbar_Dev*>(
        ::PPP_GetInterface(PPP_SCROLLBAR_DEV_INTERFACE));
  }
  return ppp_scrollbar;
}

}  // namespace

void PppScrollbarRpcServer::PPP_Scrollbar_ValueChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource resource,
    int32_t value) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  const PPP_Scrollbar_Dev* ppp_scrollbar = PPPScrollbar();
  if (ppp_scrollbar == NULL || ppp_scrollbar->ValueChanged == NULL)
    return;
  ppp_scrollbar->ValueChanged(instance, resource, value);

  DebugPrintf("PPP_Scrollbar::ValueChanged: "
              "value=%"NACL_PRId32"\n", value);
  rpc->result = NACL_SRPC_RESULT_OK;
}

