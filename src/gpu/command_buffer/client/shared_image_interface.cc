// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_interface.h"

#include "base/notreached.h"

namespace gpu {

uint32_t SharedImageInterface::UsageForMailbox(const Mailbox& mailbox) {
  return 0u;
}

void SharedImageInterface::NotifyMailboxAdded(const Mailbox& /*mailbox*/,
                                              uint32_t /*usage*/) {}

std::vector<Mailbox> SharedImageInterface::CreateSharedImageVideoPlanes(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    uint32_t usage) {
  NOTREACHED();
  return {};
}

Mailbox SharedImageInterface::CreateSharedImageWithAHB(
    const Mailbox& mailbox,
    uint32_t usage,
    const SyncToken& sync_token) {
  NOTREACHED();
  return Mailbox();
}

Mailbox SharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  return CreateSharedImage(gpu_memory_buffer, gpu_memory_buffer_manager,
                           gfx::BufferPlane::DEFAULT, color_space,
                           surface_origin, alpha_type, usage);
}

void SharedImageInterface::CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  NOTREACHED();
}

}  // namespace gpu
