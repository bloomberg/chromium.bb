// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_TCPServerSocket_Private functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBTCPServerSocketPrivateInterface;

namespace {

bool SuccessResourceRead(int32_t num_bytes) {
  return num_bytes >= 0;
}

nacl_abi_size_t ResourceReadSize(int32_t result) {
  UNREFERENCED_PARAMETER(result);
  return static_cast<nacl_abi_size_t>(sizeof(PP_Resource));
}

}  // namespace

void PpbTCPServerSocketPrivateRpcServer::PPB_TCPServerSocket_Private_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *resource = PPBTCPServerSocketPrivateInterface()->Create(instance);

  DebugPrintf("PPB_TCPServerSocket_Private::Create: " \
              "resource=%"NACL_PRId32"\n", *resource);
}

void PpbTCPServerSocketPrivateRpcServer::
PPB_TCPServerSocket_Private_IsTCPServerSocket(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource resource,
    // output
    int32_t* is_tcp_server_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  PP_Bool pp_success =
      PPBTCPServerSocketPrivateInterface()->IsTCPServerSocket(resource);
  *is_tcp_server_socket = PP_ToBool(pp_success);

  DebugPrintf("PPB_TCPServerSocket_Private::IsTCPServerSocket: " \
              "is_tcp_server_socket=%d\n", *is_tcp_server_socket);
}

void PpbTCPServerSocketPrivateRpcServer::PPB_TCPServerSocket_Private_Listen(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource tcp_server_socket,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t backlog,
    int32_t callback_id,
    // output
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes != static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)))
    return;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBTCPServerSocketPrivateInterface()->Listen(
      tcp_server_socket,
      reinterpret_cast<PP_NetAddress_Private*>(addr),
      backlog,
      remote_callback);

  DebugPrintf("PPB_TCPServerSocket_Private::Listen: " \
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPServerSocketPrivateRpcServer::PPB_TCPServerSocket_Private_Accept(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource tcp_server_socket,
    int32_t callback_id,
    // outputs
    PP_Resource* tcp_socket,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  char* callback_buffer = NULL;
  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel,
                                   callback_id,
                                   sizeof(PP_Resource),
                                   &callback_buffer,
                                   SuccessResourceRead,
                                   ResourceReadSize);
  *pp_error = PPBTCPServerSocketPrivateInterface()->Accept(
      tcp_server_socket,
      reinterpret_cast<PP_Resource*>(callback_buffer),
      remote_callback);

  DebugPrintf("PPB_TCPServerSocket_Private::Accept: "
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  else
    *tcp_socket = 0;

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbTCPServerSocketPrivateRpcServer::
PPB_TCPServerSocket_Private_StopListening(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource tcp_server_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  DebugPrintf("PPB_TCPServerSocket_Private::StopListening: " \
              "tcp_socket=%"NACL_PRId32"\n", tcp_server_socket);

  PPBTCPServerSocketPrivateInterface()->StopListening(tcp_server_socket);
}
