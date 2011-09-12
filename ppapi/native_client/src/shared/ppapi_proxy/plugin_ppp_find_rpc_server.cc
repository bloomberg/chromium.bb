// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Scrollbar functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPFindInterface;

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

  PP_Bool pp_supports_find = PPPFindInterface()->StartFind(
      instance,
      text,
      case_sensitive ? PP_TRUE : PP_FALSE);
  *supports_find = pp_supports_find == PP_TRUE;

  DebugPrintf("PPP_Find::StartFind: pp_supports_find=%d\n", pp_supports_find);
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

  PPPFindInterface()->SelectFindResult(instance, forward ? PP_TRUE : PP_FALSE);
  DebugPrintf("PPP_Find::SelectFindResult\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppFindRpcServer::PPP_Find_StopFind(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PPPFindInterface()->StopFind(instance);
  DebugPrintf("PPP_Find::StopFind\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}
