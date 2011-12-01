// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_tcp_socket_private.h"

#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_TCPSocket_Private::Create: instance=%"NACL_PRIu32"\n",
              instance);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Create(
          GetMainSrpcChannel(), instance, &resource);

  DebugPrintf("PPB_TCPSocket_Private::Create: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  else
    return kInvalidResourceId;
}

PP_Bool IsTCPSocket(PP_Resource resource) {
  DebugPrintf("PPB_TCPSocket_Private::IsTCPSocket: "
              "resource=%"NACL_PRIu32"\n", resource);

  int32_t is_tcp_socket;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_IsTCPSocket(
          GetMainSrpcChannel(), resource, &is_tcp_socket);

  DebugPrintf("PPB_TCPSocket_Private::IsTCPSocket: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_tcp_socket)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Connect(PP_Resource tcp_socket, const char* host, uint16_t port,
                struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPSocket_Private::Connect: tcp_socket=%"NACL_PRIu32", "
              "host=%s, port=%"NACL_PRIu16"\n", tcp_socket, host, port);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Connect(
          GetMainSrpcChannel(),
          tcp_socket,
          const_cast<char*>(host),
          static_cast<int32_t>(port),
          callback_id,
          &pp_error);

  DebugPrintf("PPB_TCPSocket_Private::Connect: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t ConnectWithNetAddress(PP_Resource tcp_socket,
                              const struct PP_NetAddress_Private* addr,
                              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPSocket_Private::ConnectWithNetAddress: "
              "tcp_socket=%"NACL_PRIu32"\n", tcp_socket);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0) // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<struct PP_NetAddress_Private*>(addr));

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_ConnectWithNetAddress(
          GetMainSrpcChannel(),
          tcp_socket,
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)), raw_addr,
          callback_id,
          &pp_error);

  DebugPrintf("PPB_TCPSocket_Private::ConnectWithNetAddress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

PP_Bool GetLocalAddress(PP_Resource tcp_socket,
                        struct PP_NetAddress_Private* local_addr) {
  DebugPrintf("PPB_TCPSocket_Private::GetLocalAddress: "
              "tcp_socket=%"NACL_PRIu32"\n", tcp_socket);

  nacl_abi_size_t local_addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));

  int32_t success;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_GetLocalAddress(
          GetMainSrpcChannel(),
          tcp_socket,
          &local_addr_bytes,
          reinterpret_cast<char*>(local_addr),
          &success);

  DebugPrintf("PPB_TCPSocket_Private::GetLocalAddress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool GetRemoteAddress(PP_Resource tcp_socket,
                         struct PP_NetAddress_Private* remote_addr) {
  DebugPrintf("PPB_TCPSocket_Private::GetRemoteAddress: "
              "tcp_socket=%"NACL_PRIu32"\n", tcp_socket);

  nacl_abi_size_t remote_addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));

  int32_t success;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_GetRemoteAddress(
          GetMainSrpcChannel(),
          tcp_socket,
          &remote_addr_bytes,
          reinterpret_cast<char*>(remote_addr),
          &success);

  DebugPrintf("PPB_TCPSocket_Private::GetRemoteAddress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t SSLHandshake(PP_Resource tcp_socket,
                     const char* server_name,
                     uint16_t server_port,
                     struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPSocket_Private::SSLHandshake: "
              "tcp_socket=%"NACL_PRIu32", "
              "server_name=%s, server_port=%"NACL_PRIu16"\n",
              tcp_socket, server_name, server_port);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0) // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_SSLHandshake(
          GetMainSrpcChannel(),
          tcp_socket,
          const_cast<char*>(server_name),
          static_cast<int32_t>(server_port),
          callback_id,
          &pp_error);

  DebugPrintf("PPB_TCPSocket_Private::SSLHandshake: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Read(PP_Resource tcp_socket,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPSocket_Private::Read: tcp_socket=%"NACL_PRIu32", "
              "bytes_to_read=%"NACL_PRId32"\n", tcp_socket, bytes_to_read);

  if (bytes_to_read < 0)
    bytes_to_read = 0;
  nacl_abi_size_t buffer_size = bytes_to_read;

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback, buffer);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error_or_bytes;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Read(
          GetMainSrpcChannel(),
          tcp_socket,
          bytes_to_read,
          callback_id,
          &buffer_size, buffer,
          &pp_error_or_bytes);

  DebugPrintf("PPB_TCPSocket_Private::Read: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

int32_t Write(PP_Resource tcp_socket,
              const char* buffer,
              int32_t bytes_to_write,
              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPSocket_Private::Write: tcp_socket=%"NACL_PRIu32", "
              "bytes_to_write=%"NACL_PRId32"\n", tcp_socket, bytes_to_write);

  if (bytes_to_write < 0)
    bytes_to_write = 0;
  nacl_abi_size_t buffer_size = static_cast<nacl_abi_size_t>(bytes_to_write);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error_or_bytes = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Write(
          GetMainSrpcChannel(),
          tcp_socket,
          buffer_size, const_cast<char*>(buffer),
          bytes_to_write,
          callback_id,
          &pp_error_or_bytes);

  DebugPrintf("PPB_TCPSocket_Private::Write: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

void Disconnect(PP_Resource tcp_socket) {
  DebugPrintf("PPB_TCPSocket_Private::Disconnect: tcp_socket="NACL_PRIu32"\n",
              tcp_socket);

  NaClSrpcError srpc_result =
      PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Disconnect(
          GetMainSrpcChannel(), tcp_socket);
  DebugPrintf("PPB_TCPSocket_Private::Disconnect: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_TCPSocket_Private* PluginTCPSocketPrivate::GetInterface() {
  static const PPB_TCPSocket_Private tcpsocket_private_interface = {
    Create,
    IsTCPSocket,
    Connect,
    ConnectWithNetAddress,
    GetLocalAddress,
    GetRemoteAddress,
    SSLHandshake,
    Read,
    Write,
    Disconnect,
  };
  return &tcpsocket_private_interface;
}

}  // namespace ppapi_proxy
