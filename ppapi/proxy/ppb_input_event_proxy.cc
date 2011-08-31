// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_input_event_proxy.h"

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_InputEvent_API;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateInputEventProxy(Dispatcher* dispatcher,
                                      const void* target_interface) {
  return new PPB_InputEvent_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_InputEvent_Proxy::PPB_InputEvent_Proxy(Dispatcher* dispatcher,
                                           const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_InputEvent_Proxy::~PPB_InputEvent_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetInputEventInfo() {
  static const Info info = {
    thunk::GetPPB_InputEvent_Thunk(),
    PPB_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetKeyboardInputEventInfo() {
  static const Info info = {
    thunk::GetPPB_KeyboardInputEvent_Thunk(),
    PPB_KEYBOARD_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetMouseInputEventInfo1_0() {
  static const Info info = {
    thunk::GetPPB_MouseInputEvent_1_0_Thunk(),
    PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetMouseInputEventInfo1_1() {
  static const Info info = {
    thunk::GetPPB_MouseInputEvent_1_1_Thunk(),
    PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetWheelInputEventInfo() {
  static const Info info = {
    thunk::GetPPB_WheelInputEvent_Thunk(),
    PPB_WHEEL_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

bool PPB_InputEvent_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // There are no IPC messages for this interface.
  NOTREACHED();
  return false;
}

}  // namespace proxy
}  // namespace ppapi
