// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/mojo_gpu_memory_buffer_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"

namespace ui {

MojoGpuMemoryBufferManager::MojoGpuMemoryBufferManager()
    : weak_ptr_factory_(this) {}

MojoGpuMemoryBufferManager::~MojoGpuMemoryBufferManager() {}

void MojoGpuMemoryBufferManager::DeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gpu::SyncToken& sync_token) {
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  // TODO(sad): Get the memory buffer handle from GpuService.
  size_t bytes = gfx::BufferSizeForBufferFormat(size, format);

  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(bytes);
  if (!handle.is_valid())
    return nullptr;

  base::SharedMemoryHandle shm_handle;
  size_t shared_memory_size;
  bool readonly;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &shm_handle, &shared_memory_size, &readonly);
  if (result != MOJO_RESULT_OK)
    return nullptr;
  DCHECK_EQ(shared_memory_size, bytes);

  const int stride = base::checked_cast<int>(
      gfx::RowSizeForBufferFormat(size.width(), format, 0));

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;
  gmb_handle.id = gfx::GpuMemoryBufferId(++counter_);
  gmb_handle.handle = shm_handle;
  gmb_handle.offset = 0;
  gmb_handle.stride = stride;

  std::unique_ptr<gpu::GpuMemoryBufferImpl> buffer(
      gpu::GpuMemoryBufferImpl::CreateFromHandle(
          gmb_handle, size, format, usage,
          base::Bind(&MojoGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                     weak_ptr_factory_.GetWeakPtr(), gmb_handle.id)));
  if (!buffer)
    return nullptr;

  return std::move(buffer);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::GpuMemoryBuffer*
MojoGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return gpu::GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void MojoGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

}  // namespace ui
