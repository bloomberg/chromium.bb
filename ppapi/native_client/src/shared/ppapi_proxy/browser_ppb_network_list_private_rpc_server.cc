// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_NetworkList_Private functions.

#include <string.h>
#include <algorithm>
#include <limits>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBNetworkListPrivateInterface;
using ppapi_proxy::SerializeTo;

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_IsNetworkList(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* out_bool) {
  DebugPrintf("PPB_NetworkList_Private::IsNetworkList: "
              "resource=%"NACL_PRId32"\n", resource);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      PPBNetworkListPrivateInterface()->IsNetworkList(resource);
  *out_bool = PP_ToBool(pp_success);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* count) {
  DebugPrintf("PPB_NetworkList_Private::GetCount: "
              "resource=%"NACL_PRId32"\n", resource);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *count = PPBNetworkListPrivateInterface()->GetCount(resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetName(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t index,
    nacl_abi_size_t* name_bytes, char* name) {
  DebugPrintf("PPB_NetworkList_Private::GetName: "
              "resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var pp_string =
      PPBNetworkListPrivateInterface()->GetName(resource, index);
  if (SerializeTo(&pp_string, name, name_bytes))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetType(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t index,
    int32_t* type) {
  DebugPrintf("PPB_NetworkList_Private::GetType: "
              "resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *type = PPBNetworkListPrivateInterface()->GetType(resource, index);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetState(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t index,
    int32_t* state) {
  DebugPrintf("PPB_NetworkList_Private::GetState: "
              "resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *state = PPBNetworkListPrivateInterface()->GetState(resource, index);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetIpAddresses(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t index,
    nacl_abi_size_t* addr_bytes, char* addr,
    int32_t* result) {
  DebugPrintf("PPB_NetworkList_Private::GetIpAddresses: "
              "resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  int addr_buffer_size = *addr_bytes / sizeof(PP_NetAddress_Private);
  *result = PPBNetworkListPrivateInterface()->GetIpAddresses(
      resource, index,
      reinterpret_cast<PP_NetAddress_Private*>(addr), addr_buffer_size);

  *addr_bytes = std::min(
      static_cast<nacl_abi_size_t>(*result * sizeof(PP_NetAddress_Private)),
      *addr_bytes);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetDisplayName(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t index,
    nacl_abi_size_t* display_name_bytes, char* display_name) {
  DebugPrintf("PPB_NetworkList_Private::GetDisplayName: "
              "resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var pp_string =
      PPBNetworkListPrivateInterface()->GetDisplayName(resource, index);

  if (SerializeTo(&pp_string, display_name, display_name_bytes))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetworkListPrivateServer::PPB_NetworkList_Private_GetMTU(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t index,
    int32_t* mtu) {
  DebugPrintf("PPB_NetworkList_Private::GetMTU: "
              "resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *mtu = PPBNetworkListPrivateInterface()->GetMTU(resource, index);

  rpc->result = NACL_SRPC_RESULT_OK;
}
