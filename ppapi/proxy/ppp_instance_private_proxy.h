// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_INSTANCE_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPP_INSTANCE_PRIVATE_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPP_Instance_Private;

namespace ppapi {
namespace proxy {

class SerializedVarReturnValue;

class PPP_Instance_Private_Proxy : public InterfaceProxy {
 public:
  PPP_Instance_Private_Proxy(Dispatcher* dispatcher,
                             const void* target_interface);
  virtual ~PPP_Instance_Private_Proxy();

  static const Info* GetInfo();

  const PPP_Instance_Private* ppp_instance_private_target() const {
    return reinterpret_cast<const PPP_Instance_Private*>(target_interface());
  }

 private:
  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnMsgGetInstanceObject(PP_Instance instance,
                              SerializedVarReturnValue result);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_INSTANCE_PRIVATE_PROXY_H_
