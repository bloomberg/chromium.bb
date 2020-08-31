// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gl {
class GLImage;
}

namespace gpu {

class GPU_EXPORT ImageFactory {
 public:
  ImageFactory();

  // Creates a GLImage instance for GPU memory buffer identified by |handle|.
  // |client_id| should be set to the client requesting the creation of instance
  // and can be used by factory implementation to verify access rights.
  virtual scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id,
      SurfaceHandle surface_handle) = 0;

  // Create an anonymous GLImage backed by a GpuMemoryBuffer that doesn't have a
  // client_id. It can't be passed to other processes.
  virtual bool SupportsCreateAnonymousImage() const;
  virtual scoped_refptr<gl::GLImage> CreateAnonymousImage(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      SurfaceHandle surface_handle,
      bool* is_cleared);

  // An image can only be bound to a texture with the appropriate type.
  virtual unsigned RequiredTextureType();

  // Whether a created image can have format GL_RGB.
  virtual bool SupportsFormatRGB();

 protected:
  virtual ~ImageFactory();
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_
