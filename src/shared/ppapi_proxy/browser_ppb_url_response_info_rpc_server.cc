// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_URLResponseInfo functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "srpcgen/ppb_rpc.h"

void PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_IsURLResponseInfo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // outputs
    int32_t* is_url_response_info) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  PP_Bool pp_is_url_response_info =
      ppapi_proxy::PPBURLResponseInfoInterface()->IsURLResponseInfo(resource);
  *is_url_response_info = (pp_is_url_response_info == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_GetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Resource response,
      int32_t property,
      // outputs
      nacl_abi_size_t* value_size, char* value_bytes) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_Var value =
      ppapi_proxy::PPBURLResponseInfoInterface()->GetProperty(
          response,
          static_cast<PP_URLResponseProperty>(property));
  if (!ppapi_proxy::SerializeTo(&value, value_bytes, value_size))
    return;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_GetBodyAsFileRef(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Resource response,
      // outputs
      PP_Resource* file_ref) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  *file_ref =
      ppapi_proxy::PPBURLResponseInfoInterface()->GetBodyAsFileRef(response);
  rpc->result = NACL_SRPC_RESULT_OK;
}
