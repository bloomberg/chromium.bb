/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CONTEXT_3D_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CONTEXT_3D_H_

#include "base/memory/scoped_ptr.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_context_3d_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_opengles_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_instance.h"

namespace gpu {
class CommandBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace ppapi_proxy {

// Implements the plugin (i.e., .nexe) side of the PPB_Context3D interface.
class PluginContext3D : public PluginResource {
 public:
  PluginContext3D();
  ~PluginContext3D();

  static const PPB_Context3D_Dev* GetInterface();
  static const PPB_OpenGLES2_Dev* GetOpenGLESInterface();

  virtual bool InitFromBrowserResource(PP_Resource resource);

  gpu::gles2::GLES2Implementation* impl() {
    return gles2_implementation_.get();
  }

  void SwapBuffers();

  PP_Instance instance_id() { return instance_id_; }
  void set_instance_id(PP_Instance instance_id) { instance_id_ = instance_id; }

  static inline gpu::gles2::GLES2Implementation* implFromResource(
      PP_Resource context) {
    if (cached_resource == context && cached_implementation != NULL)
      return cached_implementation;

    return implFromResourceSlow(context);
  }


 private:
  static __thread PP_Resource cached_resource;
  static __thread gpu::gles2::GLES2Implementation* cached_implementation;

  // GLES2 Implementation objects.
  scoped_ptr<gpu::CommandBuffer> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  PP_Instance instance_id_;

  static gpu::gles2::GLES2Implementation* implFromResourceSlow(
      PP_Resource context);

  IMPLEMENT_RESOURCE(PluginContext3D);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginContext3D);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CONTEXT_3D_H_
