// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_find.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

void NumberOfFindResultsChanged(PP_Instance instance,
                                int32_t total,
                                PP_Bool final_result) {
  DebugPrintf("PPB_Find::NumberOfFindResultsChanged: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PpbFindRpcClient::PPB_Find_NumberOfFindResultsChanged(
          GetMainSrpcChannel(),
          instance,
          total,
          final_result == PP_TRUE);

  DebugPrintf("PPB_Find::NumberOfFindResultsChanged: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void SelectedFindResultChanged(PP_Instance instance,
                               int32_t index) {
  DebugPrintf("PPB_Find::SelectedFindResultChanged: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PpbFindRpcClient::PPB_Find_SelectedFindResultChanged(
          GetMainSrpcChannel(),
          instance,
          index);

  DebugPrintf("PPB_Find::SelectedFindResultChanged: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_Find_Dev* PluginFind::GetInterface() {
  static const PPB_Find_Dev find_interface = {
    NumberOfFindResultsChanged,
    SelectedFindResultChanged
  };
  return &find_interface;
}

}  // namespace ppapi_proxy


