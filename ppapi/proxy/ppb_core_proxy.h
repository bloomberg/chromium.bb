// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_CORE_PROXY_H_
#define PPAPI_PPB_CORE_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_Core;

namespace ppapi {
namespace proxy {

class PPB_Core_Proxy : public InterfaceProxy {
 public:
  PPB_Core_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Core_Proxy();

  static const Info* GetInfo();

  const PPB_Core* ppb_core_target() const {
    return reinterpret_cast<const PPB_Core*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgAddRefResource(const ppapi::HostResource& resource);
  void OnMsgReleaseResource(const ppapi::HostResource& resource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_CORE_PROXY_H_
