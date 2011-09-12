// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_URLRequestInfo functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::PPBURLRequestInfoInterface;
using ppapi_proxy::DeserializeTo;
using ppapi_proxy::DebugPrintf;

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource = PPBURLRequestInfoInterface()->Create(instance);
  DebugPrintf("PPB_URLRequestInfo::Create: resource=%"NACL_PRIu32"\n",
              *resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_IsURLRequestInfo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // outputs
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBURLRequestInfoInterface()->IsURLRequestInfo(resource);
  DebugPrintf("PPB_URLRequestInfo::IsURLRequestInfo: success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_SetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Resource request,
      int32_t property,
      nacl_abi_size_t value_size, char* value_bytes,
      // outputs
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var value;
  if (!DeserializeTo(rpc->channel, value_bytes, value_size, 1, &value))
    return;

  PP_Bool pp_success = PPBURLRequestInfoInterface()->SetProperty(
      request, static_cast<PP_URLRequestProperty>(property), value);
  DebugPrintf("PPB_URLRequestInfo::SetProperty: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_AppendDataToBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Resource request,
      nacl_abi_size_t data_size, char* data_bytes,
      // outputs
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBURLRequestInfoInterface()->AppendDataToBody(
      request,
      static_cast<const char*>(data_bytes),
      static_cast<uint32_t>(data_size));
  DebugPrintf("PPB_URLRequestInfo::AppendDataToBody: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_AppendFileToBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Resource request,
      PP_Resource file_ref,
      int64_t start_offset,
      int64_t number_of_bytes,
      double expected_last_modified_time,
      // outputs
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBURLRequestInfoInterface()->AppendFileToBody(
      request,
      file_ref,
      start_offset,
      number_of_bytes,
      static_cast<PP_Time>(expected_last_modified_time));
  DebugPrintf("PPB_URLRequestInfo::AppendFileToBody: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}
