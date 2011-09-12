// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Testing_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBTestingInterface;

void PpbTestingRpcServer::PPB_Testing_ReadImageData(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource device_context_2d,
    PP_Resource image,
    nacl_abi_size_t top_left_size, char* top_left,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (top_left_size != sizeof(struct PP_Point))
    return;
  struct PP_Point* pp_top_left =
      reinterpret_cast<struct PP_Point*>(top_left);

  PP_Bool pp_success =
      PPBTestingInterface()->ReadImageData(device_context_2d,
                                           image,
                                           pp_top_left);
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_Testing_Dev::ReadImageData: "
              "success=%"NACL_PRId32"\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTestingRpcServer::PPB_Testing_RunMessageLoop(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBTestingInterface()->RunMessageLoop(instance);

  DebugPrintf("PPB_Testing_Dev::RunMessageLoop: instance=%"NACL_PRIu32"\n",
              instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTestingRpcServer::PPB_Testing_QuitMessageLoop(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBTestingInterface()->QuitMessageLoop(instance);

  DebugPrintf("PPB_Testing_Dev::QuitMessageLoop: instance=%"NACL_PRIu32"\n",
              instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTestingRpcServer::PPB_Testing_GetLiveObjectsForInstance(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t* live_objects) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  uint32_t pp_live_objects =
      PPBTestingInterface()->GetLiveObjectsForInstance(instance);
  *live_objects = pp_live_objects;

  DebugPrintf("PPB_Testing_Dev::GetLiveObjectsForInstance: "
              "live_objects=%"NACL_PRIu32"\n", *live_objects);
  rpc->result = NACL_SRPC_RESULT_OK;
}


