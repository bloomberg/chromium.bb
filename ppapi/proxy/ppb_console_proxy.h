// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CONSOLE_PROXY_H_
#define PPAPI_PROXY_PPB_CONSOLE_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_var.h"

struct PPB_Console_Dev;

namespace pp {
namespace proxy {

class PPB_Console_Proxy : public InterfaceProxy {
 public:
  PPB_Console_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Console_Proxy();

  static const Info* GetInfo();

  const PPB_Console_Dev* ppb_console_target() const {
    return static_cast<const PPB_Console_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgLog(PP_Instance instance,
                int log_level,
                SerializedVarReceiveInput value);
  void OnMsgLogWithSource(PP_Instance instance,
                          int log_level,
                          SerializedVarReceiveInput source,
                          SerializedVarReceiveInput value);

  DISALLOW_COPY_AND_ASSIGN(PPB_Console_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_CONSOLE_PROXY_H_
