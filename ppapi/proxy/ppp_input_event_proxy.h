// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_INPUT_EVENT_PROXY_H_
#define PPAPI_PROXY_PPP_INPUT_EVENT_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPP_InputEvent;

namespace ppapi {

struct InputEventData;

namespace proxy {

class PPP_InputEvent_Proxy : public InterfaceProxy {
 public:
  PPP_InputEvent_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPP_InputEvent_Proxy();

  static const Info* GetInfo();

  const PPP_InputEvent* ppp_input_event_target() const {
    return static_cast<const PPP_InputEvent*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgHandleInputEvent(PP_Instance instance,
                             const ppapi::InputEventData& data);
  void OnMsgHandleFilteredInputEvent(PP_Instance instance,
                                     const ppapi::InputEventData& data,
                                     PP_Bool* result);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_INPUT_EVENT_PROXY_H_
