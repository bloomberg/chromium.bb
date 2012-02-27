// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_tcp_server_socket_private.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_TCPServerSocketPrivate::Create: "    \
              "instance=%"NACL_PRId32"\n", instance);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbTCPServerSocketPrivateRpcClient::PPB_TCPServerSocket_Private_Create(
          GetMainSrpcChannel(),
          instance,
          &resource);

  DebugPrintf("PPB_TCPServerSocketPrivate::Create: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return kInvalidResourceId;
  return resource;
}

PP_Bool IsTCPServerSocket(PP_Resource resource) {
  DebugPrintf("PPB_TCPServerSocketPrivate::IsTCPServerSocket: " \
              "resource=%"NACL_PRId32"\n", resource);

  int32_t is_tcp_server_socket;
  NaClSrpcError srpc_result =
      PpbTCPServerSocketPrivateRpcClient::
      PPB_TCPServerSocket_Private_IsTCPServerSocket(
          GetMainSrpcChannel(), resource, &is_tcp_server_socket);

  DebugPrintf("PPB_TCPServerSocket_Private::IsTCPServerSocket: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_tcp_server_socket)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Listen(PP_Resource tcp_server_socket,
               const PP_NetAddress_Private* addr,
               int32_t backlog,
               PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPServerSocket_Private::Listen: " \
              "tcp_server_socket=%"NACL_PRId32"\n", tcp_server_socket);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbTCPServerSocketPrivateRpcClient::PPB_TCPServerSocket_Private_Listen(
          GetMainSrpcChannel(),
          tcp_server_socket,
          static_cast<nacl_abi_size_t>(sizeof(*addr)),
          reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr)),
          backlog,
          callback_id,
          &pp_error);

  DebugPrintf("PPB_TCPServerSocket_Private::Listen: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Accept(PP_Resource tcp_server_socket,
               PP_Resource* tcp_socket,
               PP_CompletionCallback callback) {
  DebugPrintf("PPB_TCPServerSocket_Private::Listen: " \
              "tcp_server_socket=%"NACL_PRId32"\n", tcp_server_socket);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback,
                                                                    tcp_socket);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbTCPServerSocketPrivateRpcClient::PPB_TCPServerSocket_Private_Accept(
          GetMainSrpcChannel(),
          tcp_server_socket,
          callback_id,
          tcp_socket,
          &pp_error);

  DebugPrintf("PPB_TCPServerSocket_Private::Accept: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

void StopListening(PP_Resource tcp_server_socket) {
  DebugPrintf("PPB_TCPServerSocket_Private::StopListening: " \
              "tcp_server_socket="NACL_PRId32"\n", tcp_server_socket);

  NaClSrpcError srpc_result =
      PpbTCPServerSocketPrivateRpcClient::
      PPB_TCPServerSocket_Private_StopListening(
          GetMainSrpcChannel(), tcp_server_socket);

  DebugPrintf("PPB_TCPServerSocket_Private::Disconnect: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_TCPServerSocket_Private*
  PluginTCPServerSocketPrivate::GetInterface() {
  static const PPB_TCPServerSocket_Private tcpserversocket_private_interface = {
    Create,
    IsTCPServerSocket,
    Listen,
    Accept,
    StopListening,
  };
  return &tcpserversocket_private_interface;
}

}  // namespace ppapi_proxy
