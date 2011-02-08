// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_CURSOR_CONTROL_PROXY_H_
#define PPAPI_PPB_CURSOR_CONTROL_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_CursorControl_Dev;

namespace pp {
namespace proxy {

class PPB_CursorControl_Proxy : public InterfaceProxy {
 public:
  PPB_CursorControl_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_CursorControl_Proxy();

  static const Info* GetInfo();

  const PPB_CursorControl_Dev* ppb_cursor_control_target() const {
    return reinterpret_cast<const PPB_CursorControl_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgSetCursor(PP_Instance instance,
                      int32_t type,
                      HostResource custom_image,
                      const PP_Point& hot_spot,
                      PP_Bool* result);
  void OnMsgLockCursor(PP_Instance instance,
                       PP_Bool* result);
  void OnMsgUnlockCursor(PP_Instance instance,
                         PP_Bool* result);
  void OnMsgHasCursorLock(PP_Instance instance,
                          PP_Bool* result);
  void OnMsgCanLockCursor(PP_Instance instance,
                          PP_Bool* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_CURSOR_CONTROL_PROXY_H_
