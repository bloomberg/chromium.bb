// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_WebSocket functions.

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_websocket.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::DeserializeTo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBCoreInterface;
using ppapi_proxy::PPBWebSocketInterface;
using ppapi_proxy::Serialize;
using ppapi_proxy::SerializeTo;

void PpbWebSocketRpcServer::PPB_WebSocket_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource = PPBWebSocketInterface()->Create(instance);
  DebugPrintf("PPB_WebSocket::Create: resource=%"NACL_PRId32"\n", *resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_IsWebSocket(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // outputs
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBWebSocketInterface()->IsWebSocket(resource);
  *success = PP_ToBool(pp_success);
  DebugPrintf("PPB_WebSocket::IsWebSocket: success=%d\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_Connect(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    nacl_abi_size_t url_size,
    char* url_bytes,
    nacl_abi_size_t protocols_size,
    char* protocols_bytes,
    int32_t protocol_count,
    int32_t callback_id,
    // outputs
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  PP_Var url;
  if (!DeserializeTo(url_bytes, url_size, 1, &url))
    return;

  nacl::scoped_array<PP_Var> protocols(new PP_Var[protocol_count]);
  if (!DeserializeTo(
      protocols_bytes, protocols_size, protocol_count, protocols.get()))
    return;

  *pp_error = PPBWebSocketInterface()->Connect(
      ws,
      url,
      protocols.get(),
      static_cast<uint32_t>(protocol_count),
      remote_callback);
  DebugPrintf("PPB_WebSocket::Connect: pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_Close(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    int32_t code,
    nacl_abi_size_t reason_size,
    char* reason_bytes,
    int32_t callback_id,
    // outputs
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  PP_Var reason;
  if (!DeserializeTo(reason_bytes, reason_size, 1, &reason))
    return;

  *pp_error = PPBWebSocketInterface()->Close(
      ws, code, reason, remote_callback);
  DebugPrintf("PPB_WebSocket::Close: pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_ReceiveMessage(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    int32_t callback_id,
    // outputs
    int32_t* pp_error,
    nacl_abi_size_t* sync_read_buffer_size,
    char* sync_read_buffer_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var* callback_var = NULL;
  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id, &callback_var);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBWebSocketInterface()->ReceiveMessage(
      ws, callback_var, remote_callback);
  DebugPrintf("PPB_WebSocket::ReceiveMessage: pp_error=%"NACL_PRId32"\n",
      *pp_error);
  rpc->result = NACL_SRPC_RESULT_OK;

  // No callback scheduled. Handles synchronous completion here.
  if (*pp_error != PP_OK_COMPLETIONPENDING) {
    if (*pp_error == PP_OK) {
      // Try serialization from callback_var to sync_read_buffer_bytes. It
      // could fail if serialized callback_var is larger than
      // sync_read_buffer_size.
      if (!SerializeTo(callback_var, sync_read_buffer_bytes,
          sync_read_buffer_size)) {
        // Buffer for synchronous completion is not big enough. Uses
        // asynchronous completion callback.
        *pp_error = PP_OK_COMPLETIONPENDING;
        // Schedule to invoke remote_callback later from main thread.
        PPBCoreInterface()->CallOnMainThread(0, remote_callback, PP_OK);
        return;
      }
    }
    DeleteRemoteCallbackInfo(remote_callback);
  }
}

void PpbWebSocketRpcServer::PPB_WebSocket_SendMessage(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    nacl_abi_size_t message_size,
    char* message_bytes,
    // outputs
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var message;
  if (!DeserializeTo(message_bytes, message_size, 1, &message))
    return;

  *pp_error = PPBWebSocketInterface()->SendMessage(ws, message);
  DebugPrintf("PPB_WebSocket::SendMessage: pp_error=%"NACL_PRId32"\n",
      *pp_error);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetBufferedAmount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    int64_t* buffered_amount) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *buffered_amount = static_cast<int64_t>(
      PPBWebSocketInterface()->GetBufferedAmount(ws));
  DebugPrintf("PPB_WebSocket::GetBufferedAmount: buffered_amount=%lu\n",
      *buffered_amount);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetCloseCode(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    int32_t* close_code) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *close_code = static_cast<int32_t>(
      PPBWebSocketInterface()->GetCloseCode(ws));
  DebugPrintf("PPB_WebSocket::GetCloseCode: close_code=%d\n", *close_code);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetCloseReason(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    nacl_abi_size_t* reason_size,
    char* reason_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var reason = PPBWebSocketInterface()->GetCloseReason(ws);
  DebugPrintf("PPB_WebSocket::GetCloseReason:: reason.type=%d\n", reason.type);

  if (SerializeTo(&reason, reason_bytes, reason_size))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetCloseWasClean(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    int32_t* was_clean) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_was_clean = PPBWebSocketInterface()->GetCloseWasClean(ws);
  *was_clean = PP_ToBool(pp_was_clean);
  DebugPrintf("PPB_WebSocket::GetCloseWasClean: was_clean=%d\n", *was_clean);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetExtensions(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    nacl_abi_size_t* extensions_size,
    char* extensions_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var extensions = PPBWebSocketInterface()->GetExtensions(ws);
  DebugPrintf("PPB_WebSocket::GetExtensions:: extensions.type=%d\n",
      extensions.type);

  if (SerializeTo(&extensions, extensions_bytes, extensions_size))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetProtocol(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    nacl_abi_size_t* protocol_size,
    char* protocol_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var protocol = PPBWebSocketInterface()->GetProtocol(ws);
  DebugPrintf("PPB_WebSocket::GetProtocol:: protocol.type=%d\n",
      protocol.type);

  if (SerializeTo(&protocol, protocol_bytes, protocol_size))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetReadyState(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    int32_t* ready_state) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *ready_state = static_cast<int32_t>(
      PPBWebSocketInterface()->GetReadyState(ws));
  DebugPrintf("PPB_WebSocket::GetReadyState:: ready_state=%d\n", ready_state);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbWebSocketRpcServer::PPB_WebSocket_GetURL(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource ws,
    // outputs
    nacl_abi_size_t* url_size,
    char* url_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var url = PPBWebSocketInterface()->GetURL(ws);
  DebugPrintf("PPB_WebSocket::GetURL:: url.type=%d\n", url.type);

  if (SerializeTo(&url, url_bytes, url_size))
    rpc->result = NACL_SRPC_RESULT_OK;
}
