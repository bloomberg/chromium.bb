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
    PP_Module module,
    // outputs
    PP_Resource* resource) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource =
      ppapi_proxy::PPBURLRequestInfoInterface()->Create(module);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_IsURLRequestInfo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // outputs
    int32_t* is_url_request_info) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_is_url_request_info =
      ppapi_proxy::PPBURLRequestInfoInterface()->IsURLRequestInfo(resource);

  *is_url_request_info = (pp_is_url_request_info == PP_TRUE);
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
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var value;
  if (!ppapi_proxy::DeserializeTo(
          rpc->channel, value_bytes, value_size, 1, &value))
    return;

  PP_Bool pp_success =
      ppapi_proxy::PPBURLRequestInfoInterface()->SetProperty(
          request,
          static_cast<PP_URLRequestProperty>(property),
          value);

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
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      ppapi_proxy::PPBURLRequestInfoInterface()->AppendDataToBody(
          request,
          static_cast<const char*>(data_bytes),
          static_cast<uint32_t>(data_size));

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
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      ppapi_proxy::PPBURLRequestInfoInterface()->AppendFileToBody(
          request,
          file_ref,
          start_offset,
          number_of_bytes,
          static_cast<PP_Time>(expected_last_modified_time));

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}
