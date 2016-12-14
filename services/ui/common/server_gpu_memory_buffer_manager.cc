// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/common/server_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"

namespace ui {

ServerGpuMemoryBufferManager::ServerGpuMemoryBufferManager(
    mojom::GpuService* gpu_service,
    int client_id)
    : gpu_service_(gpu_service),
      client_id_(client_id),
      native_configurations_(gpu::GetNativeGpuMemoryBufferConfigurations()),
      weak_factory_(this) {}

ServerGpuMemoryBufferManager::~ServerGpuMemoryBufferManager() {}

gfx::GpuMemoryBufferHandle
ServerGpuMemoryBufferManager::CreateGpuMemoryBufferHandle(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(CalledOnValidThread());
  if (gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER) {
    const bool is_native = native_configurations_.find(std::make_pair(
                               format, usage)) != native_configurations_.end();
    if (is_native) {
      gfx::GpuMemoryBufferHandle handle;
      gpu_service_->CreateGpuMemoryBuffer(id, size, format, usage, client_id,
                                          surface_handle, &handle);
      if (!handle.is_null())
        native_buffers_[client_id].insert(handle.id);
      return handle;
    }
  }

  DCHECK(gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage))
      << static_cast<int>(usage);
  return gpu::GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(id, size,
                                                                     format);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
ServerGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  gfx::GpuMemoryBufferId id(next_gpu_memory_id_++);
  gfx::GpuMemoryBufferHandle handle = CreateGpuMemoryBufferHandle(
      id, client_id_, size, format, usage, surface_handle);
  if (handle.is_null())
    return nullptr;
  return gpu::GpuMemoryBufferImpl::CreateFromHandle(
      handle, size, format, usage,
      base::Bind(&ServerGpuMemoryBufferManager::DestroyGpuMemoryBuffer,
                 weak_factory_.GetWeakPtr(), id, client_id_));
}

void ServerGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  DCHECK(CalledOnValidThread());
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

void ServerGpuMemoryBufferManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK(CalledOnValidThread());
  if (native_buffers_[client_id].erase(id))
    gpu_service_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void ServerGpuMemoryBufferManager::DestroyAllGpuMemoryBufferForClient(
    int client_id) {
  DCHECK(CalledOnValidThread());
  for (gfx::GpuMemoryBufferId id : native_buffers_[client_id])
    gpu_service_->DestroyGpuMemoryBuffer(id, client_id, gpu::SyncToken());
  native_buffers_.erase(client_id);
}

}  // namespace ui
