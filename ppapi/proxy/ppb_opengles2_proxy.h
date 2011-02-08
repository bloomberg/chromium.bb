// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_OPENGLES2_PROXY_H_
#define PPAPI_PPB_OPENGLES2_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"

struct PPB_OpenGLES2_Dev;

namespace pp {
namespace proxy {

class PPB_OpenGLES2_Proxy : public InterfaceProxy {
 public:
  PPB_OpenGLES2_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_OpenGLES2_Proxy();

  static const Info* GetInfo();

  const PPB_OpenGLES2_Dev* ppb_gles2_target() const {
    return reinterpret_cast<const PPB_OpenGLES2_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_OPENGLES2_PROXY_H_
