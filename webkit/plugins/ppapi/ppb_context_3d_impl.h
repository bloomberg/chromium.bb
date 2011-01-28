// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_

#include "base/scoped_ptr.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

namespace gpu {
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace webkit {
namespace ppapi {

class PPB_Surface3D_Impl;

class PPB_Context3D_Impl : public Resource {
 public:
  explicit PPB_Context3D_Impl(PluginInstance* instance);
  virtual ~PPB_Context3D_Impl();

  static const PPB_Context3D_Dev* GetInterface();

  // Resource override.
  virtual PPB_Context3D_Impl* AsPPB_Context3D_Impl();

  bool Init(PP_Config3D_Dev config,
            PP_Resource share_context,
            const int32_t* attrib_list);

  PluginInstance* instance() {
    return instance_;
  }

  PluginDelegate::PlatformContext3D* platform_context() {
    return platform_context_.get();
  }

  gpu::gles2::GLES2Implementation* gles2_impl() {
    return gles2_impl_.get();
  }

  int32_t BindSurfaces(PPB_Surface3D_Impl* draw,
                       PPB_Surface3D_Impl* read);

 private:
  void Destroy();

  // Plugin instance this context is associated with.
  PluginInstance* instance_;

  // PluginDelegate's 3D Context. Responsible for providing the command buffer.
  scoped_ptr<PluginDelegate::PlatformContext3D> platform_context_;

  scoped_ptr<gpu::gles2::GLES2CmdHelper> helper_;
  int32 transfer_buffer_id_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;

  PPB_Surface3D_Impl* draw_surface_;
  PPB_Surface3D_Impl* read_surface_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Context3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_
