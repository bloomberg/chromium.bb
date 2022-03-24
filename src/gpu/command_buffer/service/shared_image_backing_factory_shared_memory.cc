// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_shared_memory.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_shared_memory.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

SharedImageBackingFactorySharedMemory::SharedImageBackingFactorySharedMemory() =
    default;

SharedImageBackingFactorySharedMemory::
    ~SharedImageBackingFactorySharedMemory() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactorySharedMemory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactorySharedMemory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactorySharedMemory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER);
  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, format)) {
    return nullptr;
  }

  auto backing = std::make_unique<SharedImageBackingSharedMemory>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(shm_wrapper));
  return backing;
}

bool SharedImageBackingFactorySharedMemory::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  if (gmb_type != gfx::SHARED_MEMORY_BUFFER) {
    return false;
  }

  if (usage != SHARED_IMAGE_USAGE_CPU_WRITE) {
    return false;
  }

  *allow_legacy_mailbox = false;
  return true;
}

}  // namespace gpu
