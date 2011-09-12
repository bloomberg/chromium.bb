// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_opengles2_proxy.h"

#include "ppapi/shared_impl/opengles2_impl.h"

namespace ppapi {
namespace proxy {

namespace {
InterfaceProxy* CreateOpenGLES2Proxy(Dispatcher* dispatcher,
                                     const void* target_interface) {
  return new PPB_OpenGLES2_Proxy(dispatcher, target_interface);
}
}  // namespace

PPB_OpenGLES2_Proxy::PPB_OpenGLES2_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_OpenGLES2_Proxy::~PPB_OpenGLES2_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_OpenGLES2_Proxy::GetInfo() {
  static const Info info = {
    ppapi::OpenGLES2Impl::GetInterface(),
    PPB_OPENGLES2_INTERFACE,
    INTERFACE_ID_PPB_OPENGLES2,
    false,
    &CreateOpenGLES2Proxy,
  };
  return &info;
}

bool PPB_OpenGLES2_Proxy::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

}  // namespace proxy
}  // namespace ppapi
