// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_VAR_PROXY_H_
#define PPAPI_PPB_VAR_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"

struct PPB_Var;

namespace ppapi {
namespace proxy {

class PPB_Var_Proxy : public InterfaceProxy {
 public:
  PPB_Var_Proxy(Dispatcher* dispatcher,
                const void* target_interface);
  virtual ~PPB_Var_Proxy();

  static const Info* GetInfo();

  const PPB_Var* ppb_var_target() const {
    return static_cast<const PPB_Var*>(target_interface());
  }

  // InterfaceProxy implementation. In this case, no messages are sent or
  // received, so this always returns false.
  virtual bool OnMessageReceived(const IPC::Message& msg);

};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_VAR_PROXY_H_
