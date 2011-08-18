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

using ppapi::HostResource;
using ppapi::InputEventData;
using ppapi::InputEventImpl;
using ppapi::Resource;
using ppapi::thunk::PPB_InputEvent_API;

namespace pp {
namespace proxy {

// The implementation is actually in InputEventImpl.
class InputEvent : public Resource, public InputEventImpl {
 public:
  InputEvent(const HostResource& resource, const InputEventData& data);
  virtual ~InputEvent();

  // Resource overrides.
  virtual PPB_InputEvent_API* AsPPB_InputEvent_API() OVERRIDE;

  // InputEventImpl overrides.
  virtual PP_Var StringToPPVar(const std::string& str) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputEvent);
};

InputEvent::InputEvent(const HostResource& resource, const InputEventData& data)
    : Resource(resource),
      InputEventImpl(data) {
}

InputEvent::~InputEvent() {
}

PPB_InputEvent_API* InputEvent::AsPPB_InputEvent_API() {
  return this;
}

PP_Var InputEvent::StringToPPVar(const std::string& str) {
  return ppapi::StringVar::StringToPPVar(0, str);
}

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
    ::ppapi::thunk::GetPPB_InputEvent_Thunk(),
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
    ::ppapi::thunk::GetPPB_KeyboardInputEvent_Thunk(),
    PPB_KEYBOARD_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetMouseInputEventInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_MouseInputEvent_Thunk(),
    PPB_MOUSE_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_InputEvent_Proxy::GetWheelInputEventInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_WheelInputEvent_Thunk(),
    PPB_WHEEL_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_NONE,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

// static
PP_Resource PPB_InputEvent_Proxy::CreateProxyResource(
    PP_Instance instance,
    const InputEventData& data) {
  return (new InputEvent(HostResource::MakeInstanceOnly(instance), data))->
      GetReference();
}

bool PPB_InputEvent_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // There are no IPC messages for this interface.
  NOTREACHED();
  return false;
}

}  // namespace proxy
}  // namespace pp
