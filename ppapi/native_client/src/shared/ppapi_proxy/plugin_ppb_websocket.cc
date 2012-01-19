// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_websocket.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_WebSocket::Create: instance=%"NACL_PRId32"\n", instance);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_Create(
          GetMainSrpcChannel(), instance, &resource);
  DebugPrintf("PPB_WebSocket::Create: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  return kInvalidResourceId;
}

PP_Bool IsWebSocket(PP_Resource resource) {
  DebugPrintf(
      "PPB_WebSocket::IsWebSocket: resource=%"NACL_PRId32"\n", resource);

  int32_t is_websocket;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_IsWebSocket(
          GetMainSrpcChannel(), resource, &is_websocket);
  DebugPrintf("PPB_WebSocket::IsWebSocket: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_websocket)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Connect(PP_Resource ws,
                PP_Var url,
                const PP_Var protocols[],
                uint32_t protocol_count,
                PP_CompletionCallback callback) {
  DebugPrintf("PPB_WebSocket::Connect: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t url_size = 0;
  nacl::scoped_array<char> url_bytes(Serialize(&url, 1, &url_size));

  nacl_abi_size_t protocols_size = 0;
  nacl::scoped_array<char> protocols_bytes(
      Serialize(protocols, protocol_count, &protocols_size));

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_Connect(
          GetMainSrpcChannel(),
          ws,
          url_size, url_bytes.get(),
          protocols_size, protocols_bytes.get(),
          static_cast<int32_t>(protocol_count),
          callback_id,
          &pp_error);
  DebugPrintf("PPB_WebSocket::Connect: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Close(PP_Resource ws,
              uint16_t code,
              PP_Var reason,
              PP_CompletionCallback callback) {
  DebugPrintf("PPB_WebSocket::Close: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t reason_size = 0;
  nacl::scoped_array<char> reason_bytes(Serialize(&reason, 1, &reason_size));

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_Close(
          GetMainSrpcChannel(),
          ws,
          code,
          reason_size, reason_bytes.get(),
          callback_id,
          &pp_error);
  DebugPrintf("PPB_WebSocket::Close: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t ReceiveMessage(PP_Resource ws,
                       PP_Var* message,
                       PP_CompletionCallback callback) {
  DebugPrintf("PPB_WebSocket::ReceiveMessage: ws=%"NACL_PRId32"\n", ws);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback, message);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // TODO(toyoshim): ReceiveMessage needs performance optimization to reduce
  // chances to call RPC.
  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_ReceiveMessage(
          GetMainSrpcChannel(),
          ws,
          callback_id,
          &pp_error);
  DebugPrintf("PPB_WebSocket::ReceiveMessage: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t SendMessage(PP_Resource ws,
                    PP_Var message) {
  DebugPrintf("PPB_WebSocket::SendMessage: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t message_size = 0;
  nacl::scoped_array<char> message_bytes(
      Serialize(&message, 1, &message_size));

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_SendMessage(
          GetMainSrpcChannel(),
          ws,
          message_size, message_bytes.get(),
          &pp_error);
  DebugPrintf("PPB_WebSocket::SendMessage: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return PP_ERROR_FAILED;
  return pp_error;
}

uint64_t GetBufferedAmount(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetBufferedAmount: ws=%"NACL_PRId32"\n", ws);

  int64_t buffered_amount = 0;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetBufferedAmount(
          GetMainSrpcChannel(), ws, &buffered_amount);
  DebugPrintf("PPB_WebSocket::GetBufferedAmount: %s\n",
      NaClSrpcErrorString(srpc_result));

  return static_cast<uint64_t>(buffered_amount);
}

uint16_t GetCloseCode(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetCloseCode: ws=%"NACL_PRId32"\n", ws);

  int32_t close_code = 0;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetCloseCode(
          GetMainSrpcChannel(), ws, &close_code);
  DebugPrintf("PPB_WebSocket::GetCloseCode: %s\n",
      NaClSrpcErrorString(srpc_result));

  return static_cast<uint16_t>(close_code);
}

PP_Var GetCloseReason(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetCloseReason: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t reason_size = kMaxReturnVarSize;
  nacl::scoped_array<char> reason_bytes(new char[reason_size]);
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetCloseReason(
          GetMainSrpcChannel(), ws, &reason_size, reason_bytes.get());
  DebugPrintf("PPB_WebSocket::GetCloseReason: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK) {
    PP_Var reason = PP_MakeUndefined();
    if (DeserializeTo(reason_bytes.get(), reason_size, 1, &reason))
      return reason;
  }

  return PP_MakeUndefined();
}

PP_Bool GetCloseWasClean(PP_Resource ws) {
  DebugPrintf(
      "PPB_WebSocket::GetCloseWasClean: ws=%"NACL_PRId32"\n", ws);

  int32_t was_clean;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetCloseWasClean(
          GetMainSrpcChannel(), ws, &was_clean);
  DebugPrintf("PPB_WebSocket::GetCloseWasClean: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && was_clean)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Var GetExtensions(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetExtensions: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t extensions_size = kMaxReturnVarSize;
  nacl::scoped_array<char> extensions_bytes(new char[extensions_size]);
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetExtensions(
          GetMainSrpcChannel(), ws, &extensions_size, extensions_bytes.get());
  DebugPrintf("PPB_WebSocket::GetExtensions: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK) {
    PP_Var extensions = PP_MakeUndefined();
    if (DeserializeTo(extensions_bytes.get(), extensions_size, 1, &extensions))
      return extensions;
  }

  return PP_MakeUndefined();
}

PP_Var GetProtocol(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetProtocol: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t protocol_size = kMaxReturnVarSize;
  nacl::scoped_array<char> protocol_bytes(new char[protocol_size]);
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetProtocol(
          GetMainSrpcChannel(), ws, &protocol_size, protocol_bytes.get());
  DebugPrintf("PPB_WebSocket::GetProtocol: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK) {
    PP_Var protocol = PP_MakeUndefined();
    if (DeserializeTo(protocol_bytes.get(), protocol_size, 1, &protocol))
      return protocol;
  }

  return PP_MakeUndefined();
}

PP_WebSocketReadyState_Dev GetReadyState(PP_Resource ws) {
  DebugPrintf(
      "PPB_WebSocket::GetReadyState: ws=%"NACL_PRId32"\n", ws);

  int32_t ready_state;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetReadyState(
          GetMainSrpcChannel(), ws, &ready_state);
  DebugPrintf("PPB_WebSocket::GetReadyState: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return PP_WEBSOCKETREADYSTATE_INVALID_DEV;
  return static_cast<PP_WebSocketReadyState_Dev>(ready_state);
}

PP_Var GetURL(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetURL: ws=%"NACL_PRId32"\n", ws);

  nacl_abi_size_t url_size = kMaxReturnVarSize;
  nacl::scoped_array<char> url_bytes(new char[url_size]);
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetURL(
          GetMainSrpcChannel(), ws, &url_size, url_bytes.get());
  DebugPrintf("PPB_WebSocket::GetURL: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK) {
    PP_Var url = PP_MakeUndefined();
    if (DeserializeTo(url_bytes.get(), url_size, 1, &url))
      return url;
  }

  return PP_MakeUndefined();
}

PP_Bool SetBinaryType(PP_Resource ws,
                      PP_WebSocketBinaryType_Dev binary_type) {
  DebugPrintf("PPB_WebSocket::SetBinaryType: ws=%"NACL_PRId32"\n", ws);

  int32_t success;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_SetBinaryType(
          GetMainSrpcChannel(),
          ws,
          static_cast<int32_t>(binary_type),
          &success);
  DebugPrintf("PPB_WebSocket::SetBinaryType: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

PP_WebSocketBinaryType_Dev GetBinaryType(PP_Resource ws) {
  DebugPrintf("PPB_WebSocket::GetBinaryType: ws=%"NACL_PRId32"\n", ws);

  int32_t binary_type;
  NaClSrpcError srpc_result =
      PpbWebSocketRpcClient::PPB_WebSocket_GetBinaryType(
          GetMainSrpcChannel(),
          ws,
          &binary_type);
  DebugPrintf("PPB_WebSocket::GetBinaryType: %s\n",
      NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return PP_WEBSOCKETBINARYTYPE_INVALID;
  return static_cast<PP_WebSocketBinaryType_Dev>(binary_type);
}

}  // namespace

const PPB_WebSocket_Dev* PluginWebSocket::GetInterface() {
  static const PPB_WebSocket_Dev websocket_interface = {
    &Create,
    &IsWebSocket,
    &Connect,
    &Close,
    &ReceiveMessage,
    &SendMessage,
    &GetBufferedAmount,
    &GetCloseCode,
    &GetCloseReason,
    &GetCloseWasClean,
    &GetExtensions,
    &GetProtocol,
    &GetReadyState,
    &GetURL,
    &SetBinaryType,
    &GetBinaryType
  };
  return &websocket_interface;
}

}  // namespace ppapi_proxy
