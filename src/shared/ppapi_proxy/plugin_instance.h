/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppb_instance.h"

namespace ppapi_proxy {

// Implements the plugin side of the PPB_Instance interface.
class PluginInstance {
 public:
  PluginInstance() {}
  virtual ~PluginInstance() {}
  // The bindings for the methods invoked by the PPAPI interface.
  virtual PP_Var GetWindowObject() = 0;
  virtual PP_Var GetOwnerElementObject() = 0;
  virtual bool BindGraphics(PP_Resource device) = 0;
  virtual bool IsFullFrame() = 0;
  virtual PP_Var ExecuteScript(PP_Var script, PP_Var* exception) = 0;

  static const PPB_Instance* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_H_
