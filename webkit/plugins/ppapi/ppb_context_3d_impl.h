// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

namespace gfx {
class Size;
}

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace webkit {
namespace ppapi {

class PPB_Context3D_Impl : public Resource {
 public:
  explicit PPB_Context3D_Impl(PluginModule* module);
  virtual ~PPB_Context3D_Impl();

  static const PPB_Context3D_Dev* GetInterface();

  // Resource override.
  virtual PPB_Context3D_Impl* AsPPB_Context3D_Impl();

  bool Init(PluginInstance* instance,
            PP_Config3D_Dev config,
            PP_Resource share_context,
            const int32_t* attrib_list);

  // Associates this PPB_Context3D_Impl with the given plugin instance.
  // You can pass NULL to clear the existing device. Returns true on success.
  // In this case, the last rendered frame is displayed.
  //
  // TODO(alokp): This is confusing. This context is already associated with
  // an instance. This function should rather be called BindToInstanceGraphics
  // or something similar which means from this point on, anything drawn with
  // this context appears on instance window. This function should also not
  // take any argument. But this means modifying PPB_Instance::BindGraphics.
  bool BindToInstance(PluginInstance* new_instance);

  bool SwapBuffers();
  void SetSwapBuffersCallback(Callback0::Type* callback);

  unsigned int GetBackingTextureId();
  void ResizeBackingTexture(const gfx::Size& size);

  gpu::gles2::GLES2Implementation* gles2_impl() {
    return gles2_impl_;
  }

 private:
  void Destroy();

  // Non-owning pointer to the plugin instance this context is currently bound
  // to, if any. If the context is currently unbound, this will be NULL.
  PluginInstance* bound_instance_;

  // PluginDelegate's 3D Context. Responsible for providing the command buffer.
  scoped_ptr<PluginDelegate::PlatformContext3D> platform_context_;

  // GLES2 Implementation instance. Owned by the platform context's GGL context.
  gpu::gles2::GLES2Implementation* gles2_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Context3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_
