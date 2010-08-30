// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_GRAPHICS_3D_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_GRAPHICS_3D_H_

#include "base/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace gpu {
class CommandBuffer;
}  // namespace gpu

struct PPB_Graphics3D_Dev;
struct PPB_OpenGLES_Dev;

namespace pepper {

class Graphics3D : public Resource {
 public:
  explicit Graphics3D(PluginModule* module);

  virtual ~Graphics3D();

  static const PPB_Graphics3D_Dev* GetInterface();
  static const PPB_OpenGLES_Dev* GetOpenGLESInterface();

  static bool Shutdown();

  static Graphics3D* GetCurrent();

  static void ResetCurrent();

  // Resource override.
  virtual Graphics3D* AsGraphics3D() {
    return this;
  }

  bool Init(PP_Instance instance_id, int32_t config,
            const int32_t* attrib_list);

  bool MakeCurrent();

  bool SwapBuffers();

  gpu::gles2::GLES2Implementation* impl() {
    return gles2_implementation_.get();
  }

 private:
  void HandleRepaint(PP_Instance instance_id);
  void Destroy();

  // PluginDelegate's 3D Context. Responsible for providing the command buffer.
  scoped_ptr<PluginDelegate::PlatformContext3D> platform_context_;

  // Command buffer is owned by the platform context.
  gpu::CommandBuffer* command_buffer_;

  // GLES2 Command Helper instance.
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;

  // ID of the transfer buffer.
  int32_t transfer_buffer_id_;

  // GLES2 Implementation instance.
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;

  // Runnable methods that must be cancelled when the 3D context is destroyed.
  ScopedRunnableMethodFactory<Graphics3D> method_factory3d_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_GRAPHICS_3D_H_

