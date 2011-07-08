/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_SURFACE_3D_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_SURFACE_3D_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_surface_3d_dev.h"

namespace ppapi_proxy {

// Implements the plugin (i.e., .nexe) side of the PPB_Surface3D interface.
class PluginSurface3D : public PluginResource {
 public:
  PluginSurface3D();
  ~PluginSurface3D();

  static const PPB_Surface3D_Dev* GetInterface();

  virtual bool InitFromBrowserResource(PP_Resource resource);

  int32_t SwapBuffers(PP_Resource surface_id,
                      struct PP_CompletionCallback callback);

  void set_bound_context(PP_Resource res) { bound_context_ = res; }
  PP_Resource get_bound_context() { return bound_context_; }

 private:
  PP_Resource bound_context_;

  IMPLEMENT_RESOURCE(PluginSurface3D);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginSurface3D);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_SURFACE_3D_H_
