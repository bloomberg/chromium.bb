// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_text_input_proxy.h"

#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_instance_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

void RequestSurroundingText(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;  // Instance has gone away while message was pending.

  // Just fake out a RequestSurroundingText message to the proxy for the PPP
  // interface.
  InterfaceProxy* proxy = dispatcher->GetInterfaceProxy(API_ID_PPB_TEXT_INPUT);
  if (!proxy)
    return;
  proxy->OnMessageReceived(PpapiMsg_PPPTextInput_RequestSurroundingText(
      API_ID_PPP_TEXT_INPUT, instance,
      PPB_Instance_Shared::kExtraCharsForTextInput));
}

}  // namespace

PPB_TextInput_Proxy::PPB_TextInput_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_TextInput_Proxy::~PPB_TextInput_Proxy() {
}

ppapi::thunk::PPB_TextInput_FunctionAPI*
PPB_TextInput_Proxy::AsPPB_TextInput_FunctionAPI() {
  return this;
}

void PPB_TextInput_Proxy::SetTextInputType(PP_Instance instance,
                                           PP_TextInput_Type type) {
  dispatcher()->Send(new PpapiHostMsg_PPBTextInput_SetTextInputType(
      API_ID_PPB_TEXT_INPUT, instance, type));
}

void PPB_TextInput_Proxy::UpdateCaretPosition(PP_Instance instance,
                                              const PP_Rect& caret,
                                              const PP_Rect& bounding_box) {
  dispatcher()->Send(new PpapiHostMsg_PPBTextInput_UpdateCaretPosition(
      API_ID_PPB_TEXT_INPUT, instance, caret, bounding_box));
}

void PPB_TextInput_Proxy::CancelCompositionText(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBTextInput_CancelCompositionText(
      API_ID_PPB_TEXT_INPUT, instance));
}

void PPB_TextInput_Proxy::SelectionChanged(PP_Instance instance) {
  // The "right" way to do this is to send the message to the host. However,
  // all it will do it call RequestSurroundingText with a hardcoded number of
  // characters in response, which is an entire IPC round-trip.
  //
  // We can avoid this round-trip by just implementing the
  // RequestSurroundingText logic in the plugin process. If the logic in the
  // host becomes more complex (like a more adaptive number of characters),
  // we'll need to reevanuate whether we want to do the round trip instead.
  //
  // Be careful to post a task to avoid reentering the plugin.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RequestSurroundingText, instance));
}

void PPB_TextInput_Proxy::UpdateSurroundingText(PP_Instance instance,
                                                const char* text,
                                                uint32_t caret,
                                                uint32_t anchor) {
  dispatcher()->Send(new PpapiHostMsg_PPBTextInput_UpdateSurroundingText(
      API_ID_PPB_TEXT_INPUT, instance, text, caret, anchor));
}

bool PPB_TextInput_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_TextInput_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTextInput_SetTextInputType,
                        OnMsgSetTextInputType)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTextInput_UpdateCaretPosition,
                        OnMsgUpdateCaretPosition)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTextInput_CancelCompositionText,
                        OnMsgCancelCompositionText)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTextInput_UpdateSurroundingText,
                        OnMsgUpdateSurroundingText)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_TextInput_Proxy::OnMsgSetTextInputType(PP_Instance instance,
                                                PP_TextInput_Type type) {
  ppapi::thunk::EnterFunctionNoLock<PPB_TextInput_FunctionAPI> enter(instance,
                                                                     true);
  if (enter.succeeded())
    enter.functions()->SetTextInputType(instance, type);
}

void PPB_TextInput_Proxy::OnMsgUpdateCaretPosition(PP_Instance instance,
                                                   PP_Rect caret,
                                                   PP_Rect bounding_box) {
  ppapi::thunk::EnterFunctionNoLock<PPB_TextInput_FunctionAPI> enter(instance,
                                                                     true);
  if (enter.succeeded())
    enter.functions()->UpdateCaretPosition(instance, caret, bounding_box);
}

void PPB_TextInput_Proxy::OnMsgCancelCompositionText(PP_Instance instance) {
  ppapi::thunk::EnterFunctionNoLock<PPB_TextInput_FunctionAPI> enter(instance,
                                                                     true);
  if (enter.succeeded())
    enter.functions()->CancelCompositionText(instance);
}

void PPB_TextInput_Proxy::OnMsgUpdateSurroundingText(PP_Instance instance,
                                                     const std::string& text,
                                                     uint32_t caret,
                                                     uint32_t anchor) {
  ppapi::thunk::EnterFunctionNoLock<PPB_TextInput_FunctionAPI> enter(instance,
                                                                     true);
  if (enter.succeeded())
    enter.functions()->UpdateSurroundingText(instance,
                                             text.c_str(), caret, anchor);
}

}  // namespace proxy
}  // namespace ppapi
