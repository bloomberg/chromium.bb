// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_INSTANCE_PROXY_H_
#define PPAPI_PROXY_PPP_INSTANCE_PROXY_H_

#include <string>
#include <vector>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PP_InputEvent;
struct PP_Rect;
struct PPP_Instance;

namespace pp {
namespace proxy {

class SerializedVarReturnValue;

class PPP_Instance_Proxy : public InterfaceProxy {
 public:
  PPP_Instance_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPP_Instance_Proxy();

  const PPP_Instance* ppp_instance_target() const {
    return reinterpret_cast<const PPP_Instance*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgDidCreate(PP_Instance instance,
                      const std::vector<std::string>& argn,
                      const std::vector<std::string>& argv,
                      PP_Bool* result);
  void OnMsgDidDestroy(PP_Instance instance);
  void OnMsgDidChangeView(PP_Instance instance,
                          const PP_Rect& position,
                          const PP_Rect& clip);
  void OnMsgDidChangeFocus(PP_Instance instance, PP_Bool has_focus);
  void OnMsgHandleInputEvent(PP_Instance instance,
                             const PP_InputEvent& event,
                             PP_Bool* result);
  void OnMsgHandleDocumentLoad(PP_Instance instance,
                               const HostResource& url_loader,
                               PP_Bool* result);
  void OnMsgGetInstanceObject(PP_Instance instance,
                              SerializedVarReturnValue result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPP_INSTANCE_PROXY_H_
