// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_websocket_impl.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"

using ppapi::thunk::PPB_WebSocket_API;

namespace webkit {
namespace ppapi {

PPB_WebSocket_Impl::PPB_WebSocket_Impl(PP_Instance instance)
    : Resource(instance) {
}

PPB_WebSocket_Impl::~PPB_WebSocket_Impl() {
}

// static
PP_Resource PPB_WebSocket_Impl::Create(PP_Instance instance) {
  scoped_refptr<PPB_WebSocket_Impl> ws(new PPB_WebSocket_Impl(instance));
  return ws->GetReference();
}

PPB_WebSocket_API* PPB_WebSocket_Impl::AsPPB_WebSocket_API() {
  return this;
}

int32_t PPB_WebSocket_Impl::Connect(PP_Var url,
                                    const PP_Var protocols[],
                                    uint32_t protocol_count,
                                    PP_CompletionCallback callback) {
  // TODO(toyoshim): Implement it.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t PPB_WebSocket_Impl::Close(uint16_t code,
                                  PP_Var reason,
                                  PP_CompletionCallback callback) {
  // TODO(toyoshim): Implement it.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t PPB_WebSocket_Impl::ReceiveMessage(PP_Var* message,
                                           PP_CompletionCallback callback) {
  // TODO(toyoshim): Implement it.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t PPB_WebSocket_Impl::SendMessage(PP_Var message) {
  // TODO(toyoshim): Implement it.
  return PP_ERROR_NOTSUPPORTED;
}

uint64_t PPB_WebSocket_Impl::GetBufferedAmount() {
  // TODO(toyoshim): Implement it.
  return 0;
}

uint16_t PPB_WebSocket_Impl::GetCloseCode() {
  // TODO(toyoshim): Implement it.
  return 0;
}

PP_Var PPB_WebSocket_Impl::GetCloseReason() {
  // TODO(toyoshim): Implement it.
  return PP_MakeUndefined();
}

PP_Bool PPB_WebSocket_Impl::GetCloseWasClean() {
  // TODO(toyoshim): Implement it.
  return PP_FALSE;
}

PP_Var PPB_WebSocket_Impl::GetExtensions() {
  // TODO(toyoshim): Implement it.
  return PP_MakeUndefined();
}

PP_Var PPB_WebSocket_Impl::GetProtocol() {
  // TODO(toyoshim): Implement it.
  return PP_MakeUndefined();
}

PP_WebSocketReadyState_Dev PPB_WebSocket_Impl::GetReadyState() {
  // TODO(toyoshim): Implement it.
  return PP_WEBSOCKETREADYSTATE_INVALID_DEV;
}

PP_Var PPB_WebSocket_Impl::GetURL() {
  // TODO(toyoshim): Implement it.
  return PP_MakeUndefined();
}

}  // namespace ppapi
}  // namespace webkit
