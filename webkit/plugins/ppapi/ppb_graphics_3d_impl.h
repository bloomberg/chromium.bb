// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "gfx/size.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_instance.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

struct PPB_Graphics3D_Dev;
struct PPB_OpenGLES2_Dev;

namespace webkit {
namespace ppapi {

class PPB_Graphics3D_Impl : public Resource {
 public:
  explicit PPB_Graphics3D_Impl(PluginModule* module);

  virtual ~PPB_Graphics3D_Impl();

  static const PPB_Graphics3D_Dev* GetInterface();
  static const PPB_OpenGLES2_Dev* GetOpenGLES2Interface();

  static bool Shutdown();

  // Resource override.
  virtual PPB_Graphics3D_Impl* AsPPB_Graphics3D_Impl();

  bool Init(PP_Instance instance_id, int32_t config,
            const int32_t* attrib_list);

  // Associates this PPB_Graphics3D_Impl with the given plugin instance. You can pass
  // NULL to clear the existing device. Returns true on success. In this case,
  // the last rendered frame is displayed.
  // TODO(apatrick): Figure out the best semantics here.
  bool BindToInstance(PluginInstance* new_instance);

  bool SwapBuffers();

  unsigned GetError();

  void ResizeBackingTexture(const gfx::Size& size);

  void SetSwapBuffersCallback(Callback0::Type* callback);

  unsigned GetBackingTextureId();

  gpu::gles2::GLES2Implementation* impl() {
    return gles2_implementation_;
  }

 private:
  void Destroy();

  // Non-owning pointer to the plugin instance this context is currently bound
  // to, if any. If the context is currently unbound, this will be NULL.
  PluginInstance* bound_instance_;

  // PluginDelegate's 3D Context. Responsible for providing the command buffer.
  scoped_ptr<PluginDelegate::PlatformContext3D> platform_context_;

  // GLES2 Implementation instance. Owned by the platform context's GGL context.
  gpu::gles2::GLES2Implementation* gles2_implementation_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

