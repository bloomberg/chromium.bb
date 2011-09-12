// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_find.h"

#include <string.h>

// Include file order cannot be observed because ppp_instance declares a
// structure return type that causes an error on Windows.
// TODO(sehr, brettw): fix the return types and include order in PPAPI.
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

namespace ppapi_proxy {

namespace {

PP_Bool StartFind(PP_Instance instance,
                  const char* text,
                  PP_Bool case_sensitive) {
  DebugPrintf("PPP_Find::StartFind: instance=%"NACL_PRIu32"\n", instance);

  int32_t supports_find = 0;
  nacl_abi_size_t text_bytes = static_cast<nacl_abi_size_t>(strlen(text));
  NaClSrpcError srpc_result = PppFindRpcClient::PPP_Find_StartFind(
      GetMainSrpcChannel(instance),
      instance,
      text_bytes, const_cast<char*>(text),
      static_cast<int32_t>(case_sensitive),
      &supports_find);

  DebugPrintf("PPP_Find::StartFind: %s\n", NaClSrpcErrorString(srpc_result));
  return supports_find ? PP_TRUE : PP_FALSE;
}

void SelectFindResult(PP_Instance instance,
                      PP_Bool forward) {
  DebugPrintf("PPP_Find::SelectFindResult: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result = PppFindRpcClient::PPP_Find_SelectFindResult(
      GetMainSrpcChannel(instance),
      instance,
      static_cast<int32_t>(forward));

  DebugPrintf("PPP_Find::SelectFindResult: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void StopFind(PP_Instance instance) {
  DebugPrintf("PPP_Find::StopFind: instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result = PppFindRpcClient::PPP_Find_StopFind(
      GetMainSrpcChannel(instance),
      instance);

  DebugPrintf("PPP_Find::StopFind: %s\n", NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_Find_Dev* BrowserFind::GetInterface() {
  static const PPP_Find_Dev find_interface = {
    StartFind,
    SelectFindResult,
    StopFind
  };
  return &find_interface;
}

}  // namespace ppapi_proxy

