// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Find_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/ppb_image_data.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBFindInterface;

void PpbFindRpcServer::PPB_Find_NumberOfFindResultsChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t total,
    int32_t final_result) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBFindInterface()->NumberOfFindResultsChanged(
      instance,
      total,
      final_result ? PP_TRUE : PP_FALSE);

  DebugPrintf("PPB_Find::NumberOfFindResultsChanged: "
              "instance=%"NACL_PRIu32"\n", instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFindRpcServer::PPB_Find_SelectedFindResultChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t index) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBFindInterface()->SelectedFindResultChanged(instance, index);

  DebugPrintf("PPB_Find::SelectedFindResultChanged: "
              "instance=%"NACL_PRIu32"\n", instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

