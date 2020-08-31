// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_OZONE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"

struct DawnProcTable;

namespace gpu {

class SharedContextState;

// Implementation of SharedImageBackingFactory that produces NativePixmap
// backed SharedImages.
class GPU_GLES2_EXPORT SharedImageBackingFactoryOzone
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryOzone(SharedContextState* shared_context_state);

  ~SharedImageBackingFactoryOzone() override;

  // SharedImageBackingFactory implementation
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      bool is_thread_safe) override;

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override;

  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override;

 private:
  SharedContextState* const shared_context_state_;
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_OZONE_H_
