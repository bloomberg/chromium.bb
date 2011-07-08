// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Scrollbar functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_find_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_resource.h"
#include "native_client/src/third_party/ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;

namespace {

const PPP_Find_Dev* PPPFind() {
  static const PPP_Find_Dev* ppp_find = NULL;
  if (ppp_find == NULL) {
    ppp_find = reinterpret_cast<const PPP_Find_Dev*>(
        ::PPP_GetInterface(PPP_FIND_DEV_INTERFACE));
  }
  return ppp_find;
}

} // namespace

void PppFindRpcServer::PPP_Find_StartFind(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    nacl_abi_size_t text_bytes, char* text,
    int32_t case_sensitive,
    int32_t* supports_find) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  const PPP_Find_Dev* ppp_find = PPPFind();
  if (ppp_find == NULL || ppp_find->StartFind == NULL)
    return;
  PP_Bool pp_supports_find = ppp_find->StartFind(
      instance,
      text,
      case_sensitive ? PP_TRUE : PP_FALSE);
  *supports_find = pp_supports_find == PP_TRUE;

  DebugPrintf("PPP_Find::StartFind");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppFindRpcServer::PPP_Find_SelectFindResult(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    int32_t forward) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  const PPP_Find_Dev* ppp_find = PPPFind();
  if (ppp_find == NULL || ppp_find->SelectFindResult == NULL)
    return;
  ppp_find->SelectFindResult(instance, forward ? PP_TRUE : PP_FALSE);

  DebugPrintf("PPP_Find::SelectFindResult");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppFindRpcServer::PPP_Find_StopFind(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  const PPP_Find_Dev* ppp_find = PPPFind();
  if (ppp_find == NULL || ppp_find->StopFind == NULL)
    return;
  ppp_find->StopFind(instance);

  DebugPrintf("PPP_Find::StopFind");
  rpc->result = NACL_SRPC_RESULT_OK;
}

