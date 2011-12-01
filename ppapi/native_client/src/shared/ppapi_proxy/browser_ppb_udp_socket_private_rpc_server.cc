// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_UDPSocket_Private functions.

#include <string.h>
#include <limits>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBUDPSocketPrivateInterface;

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Instance instance_id,
    // output
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *resource = PPBUDPSocketPrivateInterface()->Create(instance_id);

  DebugPrintf("PPB_UDPSocket_Private::Create: "
              "resource=%"NACL_PRIu32"\n", *resource);
}

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_IsUDPSocket(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource resource_id,
    // output
    int32_t* is_udp_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  PP_Bool pp_success =
      PPBUDPSocketPrivateInterface()->IsUDPSocket(resource_id);

  DebugPrintf("PPB_UDPSocket_Private::IsUDPSocket: "
              "pp_success=%d\n", pp_success);

  *is_udp_socket = (pp_success == PP_TRUE);
}

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_Bind(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource udp_socket,
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

  *pp_error = PPBUDPSocketPrivateInterface()->Bind(
      udp_socket,
      reinterpret_cast<PP_NetAddress_Private*>(addr),
      remote_callback);

  DebugPrintf("PPB_UDPSocket_Private::Bind: "
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_RecvFrom(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource udp_socket,
    int32_t num_bytes,
    int32_t callback_id,
    // output
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*buffer_bytes != static_cast<nacl_abi_size_t>(num_bytes))
    return;

  char* callback_buffer = NULL;
  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id, num_bytes,
                                   &callback_buffer);
  if (NULL == remote_callback.func)
    return;

  *pp_error_or_bytes = PPBUDPSocketPrivateInterface()->RecvFrom(
      udp_socket,
      callback_buffer,
      num_bytes,
      remote_callback);

  DebugPrintf("PPB_UDPSocket_Private::RecvFrom: "
              "pp_error_or_bytes=%"NACL_PRId32"\n", *pp_error_or_bytes);

  if (!(*pp_error_or_bytes <= num_bytes))
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

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_GetRecvFromAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource udp_socket,
    // output
    nacl_abi_size_t* addr_bytes, char* addr,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)))
    return;

  PP_Bool pp_success =
      PPBUDPSocketPrivateInterface()->GetRecvFromAddress(
          udp_socket,
          reinterpret_cast<PP_NetAddress_Private*>(addr));

  DebugPrintf("PPB_UDPSocket_Private::GetRecvFromAddress: "
              "pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_SendTo(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource udp_socket,
    nacl_abi_size_t buffer_bytes, char* buffer,
    int32_t num_bytes,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t callback_id,
    // output
    int32_t* pp_error_or_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes != static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)))
    return;
  if (!(static_cast<nacl_abi_size_t>(num_bytes) <= buffer_bytes))
    return;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error_or_bytes =
      PPBUDPSocketPrivateInterface()->SendTo(
          udp_socket,
          buffer,
          num_bytes,
          reinterpret_cast<PP_NetAddress_Private*>(addr),
          remote_callback);

  DebugPrintf("PPB_UDPSocket_Private::SendTo: "
              "pp_error_or_bytes=%"NACL_PRId32"\n", *pp_error_or_bytes);

  // Bytes must be written asynchronously.
  if (*pp_error_or_bytes != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_Close(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource udp_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  DebugPrintf("PPB_UDPSocket_Private::Close: "
              "udp_socket=%"NACL_PRIu32"\n", udp_socket);

  PPBUDPSocketPrivateInterface()->Close(udp_socket);
}
