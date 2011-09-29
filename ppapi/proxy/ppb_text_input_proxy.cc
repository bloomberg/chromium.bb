// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_text_input_proxy.h"

#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

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
      INTERFACE_ID_PPB_TEXT_INPUT, instance, type));
}

void PPB_TextInput_Proxy::UpdateCaretPosition(PP_Instance instance,
                                              const PP_Rect& caret,
                                              const PP_Rect& bounding_box) {
  dispatcher()->Send(new PpapiHostMsg_PPBTextInput_UpdateCaretPosition(
      INTERFACE_ID_PPB_TEXT_INPUT, instance, caret, bounding_box));
}

void PPB_TextInput_Proxy::CancelCompositionText(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBTextInput_CancelCompositionText(
      INTERFACE_ID_PPB_TEXT_INPUT, instance));
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

}  // namespace proxy
}  // namespace ppapi
