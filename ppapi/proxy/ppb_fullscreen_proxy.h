// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_FULLSCREEN_PROXY_H_
#define PPAPI_PPB_FULLSCREEN_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Fullscreen_Dev;

namespace pp {
namespace proxy {

class PPB_Fullscreen_Proxy : public InterfaceProxy {
 public:
  PPB_Fullscreen_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Fullscreen_Proxy();

  static const Info* GetInfo();

  const PPB_Fullscreen_Dev* ppb_fullscreen_target() const {
    return reinterpret_cast<const PPB_Fullscreen_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgIsFullscreen(PP_Instance instance,
                         PP_Bool* result);
  void OnMsgSetFullscreen(PP_Instance instance,
                          PP_Bool fullscreen,
                          PP_Bool* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_FULLSCREEN_PROXY_H_
