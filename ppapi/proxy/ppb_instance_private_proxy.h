// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_INSTANCE_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_INSTANCE_PRIVATE_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Instance_Private;

namespace pp {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Instance_Private_Proxy : public InterfaceProxy {
 public:
  PPB_Instance_Private_Proxy(Dispatcher* dispatcher,
                             const void* target_interface);
  virtual ~PPB_Instance_Private_Proxy();

  static const Info* GetInfo();

  const PPB_Instance_Private* ppb_instance_private_target() const {
    return reinterpret_cast<const PPB_Instance_Private*>(target_interface());
  }

 private:
  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnMsgGetWindowObject(PP_Instance instance,
                            SerializedVarReturnValue result);
  void OnMsgGetOwnerElementObject(PP_Instance instance,
                                  SerializedVarReturnValue result);
  void OnMsgExecuteScript(PP_Instance instance,
                          SerializedVarReceiveInput script,
                          SerializedVarOutParam out_exception,
                          SerializedVarReturnValue result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_INSTANCE_PRIVATE_PROXY_H_
