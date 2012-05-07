// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_GRAPHICS_3D_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_GRAPHICS_3D_H_

#include "base/memory/scoped_ptr.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/c/pp_instance.h"

namespace gpu {
class CommandBuffer;
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace ppapi_proxy {

// Implements the plugin (i.e., .nexe) side of the PPB_Graphics3D interface.
class PluginGraphics3D : public PluginResource {
 public:
  PluginGraphics3D();
  virtual ~PluginGraphics3D();

  static const PPB_Graphics3D* GetInterface();
  static const PPB_OpenGLES2* GetOpenGLESInterface();
  static const PPB_OpenGLES2InstancedArrays*
      GetOpenGLESInstancedArraysInterface();
  static const PPB_OpenGLES2FramebufferBlit*
      GetOpenGLESFramebufferBlitInterface();
  static const PPB_OpenGLES2FramebufferMultisample*
      GetOpenGLESFramebufferMultisampleInterface();
  static const PPB_OpenGLES2ChromiumEnableFeature*
      GetOpenGLESChromiumEnableFeatureInterface();
  static const PPB_OpenGLES2ChromiumMapSub*
      GetOpenGLESChromiumMapSubInterface();
  static const PPB_OpenGLES2Query*
      GetOpenGLESQueryInterface();

  virtual bool InitFromBrowserResource(PP_Resource graphics3d_id);

  gpu::gles2::GLES2Implementation* impl() {
    return gles2_implementation_.get();
  }

  int32_t SwapBuffers(PP_Resource graphics3d_id,
                      struct PP_CompletionCallback callback);

  PP_Instance instance_id() { return instance_id_; }
  void set_instance_id(PP_Instance instance_id) { instance_id_ = instance_id; }

  static inline gpu::gles2::GLES2Implementation* implFromResource(
      PP_Resource graphics3d_id) {
    if (cached_graphics3d_id == graphics3d_id && cached_implementation != NULL)
      return cached_implementation;

    return implFromResourceSlow(graphics3d_id);
  }


 private:
  // TODO(nfullagar): make cached_* variables TLS once 64bit NaCl is faster,
  // and the proxy has support for being called off the main thread.
  // see: http://code.google.com/p/chromium/issues/detail?id=99217
  static PP_Resource cached_graphics3d_id;
  static gpu::gles2::GLES2Implementation* cached_implementation;

  // GLES2 Implementation objects.
  scoped_ptr<gpu::CommandBuffer> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;
  scoped_ptr<gpu::TransferBuffer> transfer_buffer_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  PP_Instance instance_id_;

  static gpu::gles2::GLES2Implementation* implFromResourceSlow(
      PP_Resource context);

  IMPLEMENT_RESOURCE(PluginGraphics3D);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginGraphics3D);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_GRAPHICS_3D_H_
