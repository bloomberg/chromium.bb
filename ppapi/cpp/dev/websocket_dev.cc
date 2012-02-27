// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/websocket_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_WebSocket_Dev>() {
  return PPB_WEBSOCKET_DEV_INTERFACE;
}

}  // namespace

WebSocket_Dev::WebSocket_Dev(const InstanceHandle& instance) {
  if (!has_interface<PPB_WebSocket_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_WebSocket_Dev>()->Create(
    instance.pp_instance()));
}

WebSocket_Dev::~WebSocket_Dev() {
}

int32_t WebSocket_Dev::Connect(const Var& url, const Var protocols[],
    uint32_t protocol_count, const CompletionCallback& callback) {
  if (!has_interface<PPB_WebSocket_Dev>())
    return PP_ERROR_BADRESOURCE;

  // Convert protocols to C interface.
  PP_Var *c_protocols = NULL;
  if (protocol_count) {
    c_protocols = new PP_Var[protocol_count];
    if (!c_protocols)
      return PP_ERROR_NOMEMORY;
  }
  for (uint32_t i = 0; i < protocol_count; ++i)
    c_protocols[i] = protocols[i].pp_var();

  int32_t result = get_interface<PPB_WebSocket_Dev>()->Connect(
      pp_resource(), url.pp_var(), c_protocols, protocol_count,
      callback.pp_completion_callback());
  if (c_protocols)
    delete[] c_protocols;
  return result;
}

int32_t WebSocket_Dev::Close(uint16_t code, const Var& reason,
    const CompletionCallback& callback) {
  if (!has_interface<PPB_WebSocket_Dev>())
    return PP_ERROR_BADRESOURCE;

  return get_interface<PPB_WebSocket_Dev>()->Close(
      pp_resource(), code, reason.pp_var(),
      callback.pp_completion_callback());
}

int32_t WebSocket_Dev::ReceiveMessage(Var* message,
    const CompletionCallback& callback) {
  if (!has_interface<PPB_WebSocket_Dev>())
    return PP_ERROR_BADRESOURCE;

  return get_interface<PPB_WebSocket_Dev>()->ReceiveMessage(
      pp_resource(), const_cast<PP_Var*>(&message->pp_var()),
      callback.pp_completion_callback());
}

int32_t WebSocket_Dev::SendMessage(const Var& message) {
  if (!has_interface<PPB_WebSocket_Dev>())
    return PP_ERROR_BADRESOURCE;

  return get_interface<PPB_WebSocket_Dev>()->SendMessage(
      pp_resource(), message.pp_var());
}

uint64_t WebSocket_Dev::GetBufferedAmount() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return 0;

  return get_interface<PPB_WebSocket_Dev>()->GetBufferedAmount(pp_resource());
}

uint16_t WebSocket_Dev::GetCloseCode() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return 0;

  return get_interface<PPB_WebSocket_Dev>()->GetCloseCode(pp_resource());
}

Var WebSocket_Dev::GetCloseReason() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return 0;

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_Dev>()->GetCloseReason(pp_resource()));
}

bool WebSocket_Dev::GetCloseWasClean() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return false;

  PP_Bool result =
      get_interface<PPB_WebSocket_Dev>()->GetCloseWasClean(pp_resource());
  return PP_ToBool(result);
}

Var WebSocket_Dev::GetExtensions() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_Dev>()->GetExtensions(pp_resource()));
}

Var WebSocket_Dev::GetProtocol() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_Dev>()->GetProtocol(pp_resource()));
}

PP_WebSocketReadyState_Dev WebSocket_Dev::GetReadyState() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return PP_WEBSOCKETREADYSTATE_INVALID_DEV;

  return get_interface<PPB_WebSocket_Dev>()->GetReadyState(pp_resource());
}

Var WebSocket_Dev::GetURL() {
  if (!has_interface<PPB_WebSocket_Dev>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_Dev>()->GetURL(pp_resource()));
}

}  // namespace pp
