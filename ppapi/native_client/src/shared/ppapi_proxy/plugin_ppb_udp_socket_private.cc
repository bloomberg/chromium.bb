// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_udp_socket_private.h"

#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  DebugPrintf("PPB_UDPSocket_Private::Create: instance_id=%"NACL_PRIu32"\n",
              instance_id);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_Create(
          GetMainSrpcChannel(),
          instance_id,
          &resource);

  DebugPrintf("PPB_UDPSocket_Private::Create: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  else
    return kInvalidResourceId;
}

PP_Bool IsUDPSocket(PP_Resource resource_id) {
  DebugPrintf("PPB_UDPSocket_Private::IsUDPSocket: "
              "resource_id=%"NACL_PRIu32"\n", resource_id);

  int32_t is_udp_socket;
  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_IsUDPSocket(
          GetMainSrpcChannel(), resource_id, &is_udp_socket);

  DebugPrintf("PPB_UDPSocket_Private::IsUDPSocket: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_udp_socket)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr,
             struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_UDPSocket_Private::Bind: "
              "udp_socket=%"NACL_PRIu32"\n", udp_socket);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<struct PP_NetAddress_Private*>(addr));

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_Bind(
          GetMainSrpcChannel(),
          udp_socket,
          sizeof(PP_NetAddress_Private), raw_addr,
          callback_id,
          &pp_error);

  DebugPrintf("PPB_UDPSocket_Private::Bind: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes,
                 struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_UDPSocket_Private::RecvFrom: udp_socket=%"NACL_PRIu32", "
              "num_bytes=%"NACL_PRId32"\n", udp_socket, num_bytes);

  if (num_bytes < 0)
    num_bytes = 0;
  nacl_abi_size_t buffer_size = static_cast<nacl_abi_size_t>(num_bytes);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback, buffer);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error_or_bytes;
  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_RecvFrom(
          GetMainSrpcChannel(),
          udp_socket,
          num_bytes,
          callback_id,
          &buffer_size,
          buffer,
          &pp_error_or_bytes);

  DebugPrintf("PPB_UDPSocket_Private::RecvFrom: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

PP_Bool GetRecvFromAddress(PP_Resource udp_socket,
                           struct PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_UDPSocket_Private::GetRecvFromAddress: "
              "udp_socket="NACL_PRIu32"\n", udp_socket);

  nacl_abi_size_t addr_bytes;
  int32_t success;
  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_GetRecvFromAddress(
          GetMainSrpcChannel(),
          udp_socket,
          &addr_bytes,
          reinterpret_cast<char*>(addr),
          &success);

  DebugPrintf("PPB_UDPSocket_Private::GetRecvFromAddress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes,
               const struct PP_NetAddress_Private* addr,
               struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_UDPSocket_Private::SendTo: udp_socket="NACL_PRIu32", "
              "num_bytes="NACL_PRId32"\n", udp_socket, num_bytes);

  if (num_bytes < 0)
    num_bytes = 0;
  nacl_abi_size_t buffer_size = static_cast<nacl_abi_size_t>(num_bytes);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<struct PP_NetAddress_Private*>(addr));

  int32_t pp_error_or_bytes = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_SendTo(
          GetMainSrpcChannel(),
          udp_socket,
          buffer_size,
          const_cast<char*>(buffer),
          num_bytes,
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)),
          raw_addr,
          callback_id,
          &pp_error_or_bytes);

  DebugPrintf("PPB_UDPSocket_Private::SendTo: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

void Close(PP_Resource udp_socket) {
  DebugPrintf("PPB_UDPSocket_Private::Close: "
              "udp_socket="NACL_PRIu32"\n", udp_socket);

  NaClSrpcError srpc_result =
      PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_Close(
          GetMainSrpcChannel(),
          udp_socket);

  DebugPrintf("PPB_UDPSocket_Private::Close: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_UDPSocket_Private* PluginUDPSocketPrivate::GetInterface() {
  static const PPB_UDPSocket_Private udpsocket_private_interface = {
    Create,
    IsUDPSocket,
    Bind,
    RecvFrom,
    GetRecvFromAddress,
    SendTo,
    Close,
  };
  return &udpsocket_private_interface;
}

}  // namespace ppapi_proxy
