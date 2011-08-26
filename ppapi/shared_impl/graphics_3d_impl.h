// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_GRAPHICS_3D_IMPL_H_
#define PPAPI_SHARED_IMPL_GRAPHICS_3D_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

namespace gpu {
class CommandBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu.

namespace ppapi {

class PPAPI_SHARED_EXPORT Graphics3DImpl : public thunk::PPB_Graphics3D_API {
 public:
  // PPB_Graphics3D_API implementation.
  virtual int32_t GetAttribs(int32_t* attrib_list) OVERRIDE;
  virtual int32_t SetAttribs(int32_t* attrib_list) OVERRIDE;
  virtual int32_t ResizeBuffers(int32_t width, int32_t height) OVERRIDE;
  virtual int32_t SwapBuffers(PP_CompletionCallback callback) OVERRIDE;

  gpu::gles2::GLES2Implementation* gles2_impl() {
    return gles2_impl_.get();
  }

  // Sends swap-buffers notification to the plugin.
  void SwapBuffersACK(int32_t pp_error);

 protected:
  Graphics3DImpl();
  virtual ~Graphics3DImpl();

  virtual gpu::CommandBuffer* GetCommandBuffer() = 0;
  virtual int32 DoSwapBuffers() = 0;

  bool HasPendingSwap() { return swap_callback_.func != NULL; }
  bool CreateGLES2Impl(int32 command_buffer_size,
                       int32 transfer_buffer_size);
  void DestroyGLES2Impl();

 private:
  int32 transfer_buffer_id_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;

  // Callback that needs to be executed when swap-buffers is completed.
  PP_CompletionCallback swap_callback_;

  DISALLOW_COPY_AND_ASSIGN(Graphics3DImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_GRAPHICS_3D_IMPL_H_

