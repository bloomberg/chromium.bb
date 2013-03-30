// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_GRAPHICS_3D_IMPL_H_
#define PPAPI_SHARED_IMPL_GRAPHICS_3D_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

namespace gpu {
class CommandBuffer;
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu.

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Graphics3D_Shared
    : public Resource,
      public thunk::PPB_Graphics3D_API {
 public:
  // Resource overrides.
  virtual thunk::PPB_Graphics3D_API* AsPPB_Graphics3D_API() OVERRIDE;

  // PPB_Graphics3D_API implementation.
  virtual int32_t GetAttribs(int32_t attrib_list[]) OVERRIDE;
  virtual int32_t SetAttribs(const int32_t attrib_list[]) OVERRIDE;
  virtual int32_t GetError() OVERRIDE;
  virtual int32_t ResizeBuffers(int32_t width, int32_t height) OVERRIDE;
  virtual int32_t SwapBuffers(scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t GetAttribMaxValue(int32_t attribute, int32_t* value) OVERRIDE;

  virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLenum access) OVERRIDE;
  virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) OVERRIDE;

  gpu::gles2::GLES2Implementation* gles2_impl() {
    return gles2_impl_.get();
  }

  // Sends swap-buffers notification to the plugin.
  void SwapBuffersACK(int32_t pp_error);

 protected:
  // ScopedNoLocking makes sure we don't try to lock again when we already have
  // the proxy lock. This is used when we need to use the CommandBuffer
  // (possibly via gles2_impl) but we already have the proxy lock. The
  // CommandBuffer in the plugin side of the proxy will otherwise try to acquire
  // the ProxyLock, causing a crash because we already own the lock. (Locks in
  // Chromium are never recursive).
  class ScopedNoLocking {
   public:
    explicit ScopedNoLocking(PPB_Graphics3D_Shared* graphics3d_shared)
        : graphics3d_shared_(graphics3d_shared) {
      graphics3d_shared_->PushAlreadyLocked();
    }
    ~ScopedNoLocking() {
      graphics3d_shared_->PopAlreadyLocked();
    }
   private:
    PPB_Graphics3D_Shared* graphics3d_shared_;  // Weak

    DISALLOW_COPY_AND_ASSIGN(ScopedNoLocking);
  };


  PPB_Graphics3D_Shared(PP_Instance instance);
  PPB_Graphics3D_Shared(const HostResource& host_resource);
  virtual ~PPB_Graphics3D_Shared();

  virtual gpu::CommandBuffer* GetCommandBuffer() = 0;
  virtual int32 DoSwapBuffers() = 0;

  bool HasPendingSwap() const;
  bool CreateGLES2Impl(int32 command_buffer_size,
                       int32 transfer_buffer_size,
                       gpu::gles2::GLES2Implementation* share_gles2);
  void DestroyGLES2Impl();

 private:
  // On the plugin side, we need to know that we already have the lock, so that
  // we don't try to acquire it again. The default implementation does nothing;
  // the Plugin side of the proxy must implement these.
  friend class ScopedNoLocking;
  virtual void PushAlreadyLocked();
  virtual void PopAlreadyLocked();

  // The VideoDecoder needs to be able to call Graphics3D Flush() after taking
  // the proxy lock. Hence it needs access to ScopedNoLocking.
  friend class PPB_VideoDecoder_Shared;

  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<gpu::TransferBuffer> transfer_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;

  // Callback that needs to be executed when swap-buffers is completed.
  scoped_refptr<TrackedCallback> swap_callback_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_GRAPHICS_3D_IMPL_H_

