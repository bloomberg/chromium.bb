// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GRAPHICS_2D_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GRAPHICS_2D_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/ppb_graphics_2d.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_Graphics2D interface.
class PluginGraphics2D : public PluginResource {
 public:
  PluginGraphics2D() {}
  bool InitFromBrowserResource(PP_Resource resource) { return true; }

  static const PPB_Graphics2D* GetInterface();

 protected:
  virtual ~PluginGraphics2D() {}

 private:
  IMPLEMENT_RESOURCE(PluginGraphics2D);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginGraphics2D);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GRAPHICS_2D_H_
