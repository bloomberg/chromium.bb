// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_INPUT_EVENT_PROXY_H_
#define PPAPI_PROXY_PPB_INPUT_EVENT_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/input_event_impl.h"

namespace ppapi {

struct InputEventData;

namespace proxy {

class PPB_InputEvent_Proxy : public InterfaceProxy {
 public:
  PPB_InputEvent_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_InputEvent_Proxy();

  static const Info* GetInputEventInfo();
  static const Info* GetKeyboardInputEventInfo();
  static const Info* GetMouseInputEventInfo();
  static const Info* GetWheelInputEventInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_InputEvent_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_INPUT_EVENT_PROXY_H_
