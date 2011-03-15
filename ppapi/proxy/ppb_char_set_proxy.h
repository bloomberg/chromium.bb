// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_
#define PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_CharSet_Dev;
struct PPB_Core;

namespace pp {
namespace proxy {

class SerializedVarReturnValue;

class PPB_CharSet_Proxy : public InterfaceProxy {
 public:
  PPB_CharSet_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_CharSet_Proxy();

  static const Info* GetInfo();

  const PPB_CharSet_Dev* ppb_char_set_target() const {
    return static_cast<const PPB_CharSet_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  void OnMsgGetDefaultCharSet(PP_Instance instance,
                              SerializedVarReturnValue result);

  DISALLOW_COPY_AND_ASSIGN(PPB_CharSet_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_
