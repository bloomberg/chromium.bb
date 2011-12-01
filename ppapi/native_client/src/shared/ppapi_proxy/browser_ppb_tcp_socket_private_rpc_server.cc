// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_TCPSocket_Private functions.

#include <string.h>
#include <limits>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBTCPSocketPrivateInterface;

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Instance instance,
    // output
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *resource = PPBTCPSocketPrivateInterface()->Create(instance);

  DebugPrintf("PPB_TCPSocket_Private::Create: resource=%"NACL_PRIu32"\n",
              *resource);
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_IsTCPSocket(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource resource,
    // output
    int32_t* is_tcp_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  PP_Bool pp_success =
      PPBTCPSocketPrivateInterface()->IsTCPSocket(resource);

  DebugPrintf("PPB_TCPSocket_Private::IsTCPSocket: pp_success=%d\n",
              pp_success);

  *is_tcp_socket = (pp_success == PP_TRUE);
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Connect(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_socket,
    char* host,
    int32_t port,
    int32_t callback_id,
    // output
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBTCPSocketPrivateInterface()->Connect(
      tcp_socket,
      host,
      port,
      remote_callback);

  DebugPrintf("PPB_TCPSocket_Private::Connect: "
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_ConnectWithNetAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_socket,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t callback_id,
    // output
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes != sizeof(PP_NetAddress_Private))
    return;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBTCPSocketPrivateInterface()->ConnectWithNetAddress(
      tcp_socket,
      reinterpret_cast<struct PP_NetAddress_Private*>(addr),
      remote_callback);

  DebugPrintf("PPB_TCPSocket_Private::ConnectWithNetAddress: "
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_GetLocalAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_socket,
    // output
    nacl_abi_size_t* local_addr_bytes, char* local_addr,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*local_addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)))
    return;

  PP_Bool pp_success = PPBTCPSocketPrivateInterface()->GetLocalAddress(
      tcp_socket, reinterpret_cast<struct PP_NetAddress_Private*>(local_addr));

  DebugPrintf("PPB_TCPSocket_Private::GetLocalAddress: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_GetRemoteAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_socket,
    // output
    nacl_abi_size_t* remote_addr_bytes, char* remote_addr,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*remote_addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)))
    return;

  PP_Bool pp_success =
      PPBTCPSocketPrivateInterface()->GetRemoteAddress(
          tcp_socket,
          reinterpret_cast<struct PP_NetAddress_Private*>(remote_addr));

  DebugPrintf("PPB_TCPSocket_Private::GetRemoteAddress: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_SSLHandshake(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_socket,
    char* server_name,
    int32_t server_port,
    int32_t callback_id,
    // output
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBTCPSocketPrivateInterface()->SSLHandshake(
      tcp_socket,
      server_name,
      server_port,
      remote_callback);

  DebugPrintf("PPB_TCPSocket_Private::SSLHandshake: "
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING) // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Read(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure *done,
    // input
    PP_Resource tcp_socket,
    int32_t bytes_to_read,
    int32_t callback_id,
    // output
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*buffer_bytes != static_cast<nacl_abi_size_t>(bytes_to_read))
    return;

  char* callback_buffer = NULL;
  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id, bytes_to_read,
                                   &callback_buffer);
  if (NULL == remote_callback.func)
    return;

  *pp_error_or_bytes = PPBTCPSocketPrivateInterface()->Read(
      tcp_socket,
      callback_buffer,
      bytes_to_read,
      remote_callback);

  DebugPrintf("PPB_TCPSocket_Private::Read: "
              "pp_error_or_bytes=%"NACL_PRId32"\n", *pp_error_or_bytes);

  if (!(*pp_error_or_bytes <= bytes_to_read))
    return;

  if (*pp_error_or_bytes > 0) {  // Bytes read into |callback_buffer|.
    // No callback scheduled.
    if (!(static_cast<nacl_abi_size_t>(*pp_error_or_bytes) <= *buffer_bytes))
      return;
    *buffer_bytes = static_cast<nacl_abi_size_t>(*pp_error_or_bytes);
    memcpy(buffer, callback_buffer, *buffer_bytes);
    DeleteRemoteCallbackInfo(remote_callback);
  } else if (*pp_error_or_bytes != PP_OK_COMPLETIONPENDING) {  // Async error.
    // No callback scheduled.
    *buffer_bytes = 0;
    DeleteRemoteCallbackInfo(remote_callback);
  } else {
    // Callback scheduled.
    *buffer_bytes = 0;
  }

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Write(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource tcp_socket,
    // input
    nacl_abi_size_t buffer_bytes, char* buffer,
    int32_t bytes_to_write,
    int32_t callback_id,
    // output
    int32_t* pp_error_or_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (!(static_cast<nacl_abi_size_t>(bytes_to_write) <= buffer_bytes))
    return;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error_or_bytes = PPBTCPSocketPrivateInterface()->Write(
      tcp_socket,
      buffer,
      bytes_to_write,
      remote_callback);

  DebugPrintf("PPB_TCPSocket_Private::Write: "
              "pp_error_or_bytes=%"NACL_PRId32"\n", *pp_error_or_bytes);

  // Bytes must be written asynchronously.
  if (*pp_error_or_bytes != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Disconnect(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  DebugPrintf("PPB_TCPSocket_Private::Disconnect: tcp_socket=%"NACL_PRIu32"\n",
              tcp_socket);

  PPBTCPSocketPrivateInterface()->Disconnect(tcp_socket);
}
