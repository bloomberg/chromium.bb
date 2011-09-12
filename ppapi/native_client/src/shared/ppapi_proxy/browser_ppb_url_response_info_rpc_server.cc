// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_URLResponseInfo functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::PPBURLResponseInfoInterface;
using ppapi_proxy::SerializeTo;
using ppapi_proxy::DebugPrintf;

void PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_IsURLResponseInfo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // outputs
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);

  PP_Bool pp_success =
      PPBURLResponseInfoInterface()->IsURLResponseInfo(resource);
  DebugPrintf("PPB_URLResponseInfo::IsURLResponseInfo: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
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
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var value = PPBURLResponseInfoInterface()->GetProperty(
      response, static_cast<PP_URLResponseProperty>(property));
  DebugPrintf("PPB_URLResponseInfo::GetProperty: type=%d\n",
              value.type);

  if (!SerializeTo(&value, value_bytes, value_size))
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
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *file_ref = PPBURLResponseInfoInterface()->GetBodyAsFileRef(response);
  DebugPrintf("PPB_URLResponseInfo::GetBodyAsFileRef: file_ref="NACL_PRIu32"\n",
              *file_ref);

  rpc->result = NACL_SRPC_RESULT_OK;
}
