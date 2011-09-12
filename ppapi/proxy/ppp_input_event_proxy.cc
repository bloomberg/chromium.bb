// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_input_event_proxy.h"

#include <algorithm>

#include "ppapi/c/ppp_input_event.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_input_event_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_InputEvent_API;

namespace ppapi {
namespace proxy {

namespace {

PP_Bool HandleInputEvent(PP_Instance instance, PP_Resource input_event) {
  EnterResourceNoLock<PPB_InputEvent_API> enter(input_event, false);
  if (enter.failed()) {
    NOTREACHED();
    return PP_FALSE;
  }
  const InputEventData& data = enter.object()->GetInputEventData();
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return PP_FALSE;
  }

  // Need to send different messages depending on whether filtering is needed.
  PP_Bool result = PP_FALSE;
  if (data.is_filtered) {
    dispatcher->Send(new PpapiMsg_PPPInputEvent_HandleFilteredInputEvent(
        INTERFACE_ID_PPP_INPUT_EVENT, instance, data, &result));
  } else {
    dispatcher->Send(new PpapiMsg_PPPInputEvent_HandleInputEvent(
        INTERFACE_ID_PPP_INPUT_EVENT, instance, data));
  }
  return result;
}

static const PPP_InputEvent input_event_interface = {
  &HandleInputEvent
};

InterfaceProxy* CreateInputEventProxy(Dispatcher* dispatcher) {
  return new PPP_InputEvent_Proxy(dispatcher);
}

}  // namespace

PPP_InputEvent_Proxy::PPP_InputEvent_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_input_event_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_input_event_impl_ = static_cast<const PPP_InputEvent*>(
        dispatcher->local_get_interface()(PPP_INPUT_EVENT_INTERFACE));
  }
}

PPP_InputEvent_Proxy::~PPP_InputEvent_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_InputEvent_Proxy::GetInfo() {
  static const Info info = {
    &input_event_interface,
    PPP_INPUT_EVENT_INTERFACE,
    INTERFACE_ID_PPP_INPUT_EVENT,
    false,
    &CreateInputEventProxy,
  };
  return &info;
}

bool PPP_InputEvent_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_InputEvent_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInputEvent_HandleInputEvent,
                        OnMsgHandleInputEvent)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInputEvent_HandleFilteredInputEvent,
                        OnMsgHandleFilteredInputEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_InputEvent_Proxy::OnMsgHandleInputEvent(PP_Instance instance,
                                                 const InputEventData& data) {
  scoped_refptr<InputEventImpl> resource(new InputEventImpl(
      InputEventImpl::InitAsProxy(), instance, data));
  ppp_input_event_impl_->HandleInputEvent(instance, resource->pp_resource());
}

void PPP_InputEvent_Proxy::OnMsgHandleFilteredInputEvent(
    PP_Instance instance,
    const InputEventData& data,
    PP_Bool* result) {
  scoped_refptr<InputEventImpl> resource(new InputEventImpl(
      InputEventImpl::InitAsProxy(), instance, data));
  *result = ppp_input_event_impl_->HandleInputEvent(instance,
                                                    resource->pp_resource());
}

}  // namespace proxy
}  // namespace ppapi
