// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_
#define PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPP_MouseLock_Dev;

namespace ppapi {
namespace proxy {

class PPP_MouseLock_Proxy : public InterfaceProxy {
 public:
  PPP_MouseLock_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPP_MouseLock_Proxy();

  static const Info* GetInfo();

  const PPP_MouseLock_Dev* ppp_mouse_lock_target() const {
    return static_cast<const PPP_MouseLock_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // Message handlers.
  void OnMsgMouseLockLost(PP_Instance instance);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_
