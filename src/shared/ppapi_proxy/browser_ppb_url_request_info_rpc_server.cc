// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_URLRequestInfo functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "srpcgen/ppb_rpc.h"

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    int64_t module,
    // outputs
    int64_t* resource) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Resource pp_resource =
      ppapi_proxy::PPBURLRequestInfoInterface()->Create(
          static_cast<PP_Module>(module));

  *resource = static_cast<int64_t>(pp_resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_IsURLRequestInfo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    int64_t resource,
    // outputs
    int32_t* is_url_request_info) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_is_url_request_info =
      ppapi_proxy::PPBURLRequestInfoInterface()->IsURLRequestInfo(
          static_cast<PP_Resource>(resource));

  *is_url_request_info = (pp_is_url_request_info == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_SetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      int64_t request,
      int32_t property,
      nacl_abi_size_t value_size, char* value_bytes,
      // outputs
      int32_t* success) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var value;
  if (!ppapi_proxy::DeserializeTo(
          rpc->channel, value_bytes, value_size, 1, &value))
    return;

  PP_Bool pp_success =
      ppapi_proxy::PPBURLRequestInfoInterface()->SetProperty(
          static_cast<PP_Resource>(request),
          static_cast<PP_URLRequestProperty>(property),
          value);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_AppendDataToBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      int64_t request,
      nacl_abi_size_t data_size, char* data_bytes,
      // outputs
      int32_t* success) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      ppapi_proxy::PPBURLRequestInfoInterface()->AppendDataToBody(
          static_cast<PP_Resource>(request),
          static_cast<const char*>(data_bytes),
          static_cast<uint32_t>(data_size));

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_AppendFileToBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      int64_t request,
      int64_t file_ref,
      int64_t start_offset,
      int64_t number_of_bytes,
      double expected_last_modified_time,
      // outputs
      int32_t* success) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      ppapi_proxy::PPBURLRequestInfoInterface()->AppendFileToBody(
          static_cast<PP_Resource>(request),
          static_cast<PP_Resource>(file_ref),
          start_offset,
          number_of_bytes,
          static_cast<PP_Time>(expected_last_modified_time));

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}
