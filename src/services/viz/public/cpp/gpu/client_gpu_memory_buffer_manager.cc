// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/client_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/buffer_format_util.h"

namespace viz {
namespace {

void NotifyDestructionOnCorrectThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::GpuMemoryBufferImpl::DestructionCallback callback,
    const gpu::SyncToken& sync_token) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), sync_token));
}

}  // namespace

ClientGpuMemoryBufferManager::ClientGpuMemoryBufferManager(
    mojo::PendingRemote<mojom::GpuMemoryBufferFactory> gpu)
    : thread_("GpuMemoryThread"),
      gpu_memory_buffer_support_(
          std::make_unique<gpu::GpuMemoryBufferSupport>()) {
  CHECK(thread_.Start());
  // The thread is owned by this object. Which means the task will not run if
  // the object has been destroyed. So Unretained() is safe.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ClientGpuMemoryBufferManager::InitThread,
                                base::Unretained(this), std::move(gpu)));
}

ClientGpuMemoryBufferManager::~ClientGpuMemoryBufferManager() {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ClientGpuMemoryBufferManager::TearDownThread,
                                base::Unretained(this)));
  thread_.Stop();
}

void ClientGpuMemoryBufferManager::InitThread(
    mojo::PendingRemote<mojom::GpuMemoryBufferFactory> gpu_remote) {
  gpu_.Bind(std::move(gpu_remote));
  gpu_.set_disconnect_handler(
      base::BindOnce(&ClientGpuMemoryBufferManager::DisconnectGpuOnThread,
                     base::Unretained(this)));
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

void ClientGpuMemoryBufferManager::TearDownThread() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  DisconnectGpuOnThread();
}

void ClientGpuMemoryBufferManager::DisconnectGpuOnThread() {
  if (!gpu_.is_bound())
    return;
  gpu_.reset();
  for (auto* waiter : pending_allocation_waiters_)
    waiter->Signal();
  pending_allocation_waiters_.clear();
}

void ClientGpuMemoryBufferManager::AllocateGpuMemoryBufferOnThread(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle,
    base::WaitableEvent* wait) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());

  if (!gpu_) {
    // The Gpu interface may have been disconnected, in which case we can't
    // fulfill the request.
    wait->Signal();
    return;
  }

  // |handle| and |wait| are both on the stack, and will be alive until |wait|
  // is signaled. So it is safe for OnGpuMemoryBufferAllocated() to operate on
  // these.
  pending_allocation_waiters_.insert(wait);
  gpu_->CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(++counter_), size, format, usage,
      base::BindOnce(
          &ClientGpuMemoryBufferManager::OnGpuMemoryBufferAllocatedOnThread,
          base::Unretained(this), handle, wait));
}

void ClientGpuMemoryBufferManager::OnGpuMemoryBufferAllocatedOnThread(
    gfx::GpuMemoryBufferHandle* ret_handle,
    base::WaitableEvent* wait,
    gfx::GpuMemoryBufferHandle handle) {
  auto it = pending_allocation_waiters_.find(wait);
  DCHECK(it != pending_allocation_waiters_.end());
  pending_allocation_waiters_.erase(it);

  *ret_handle = std::move(handle);
  wait->Signal();
}

void ClientGpuMemoryBufferManager::DeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gpu::SyncToken& sync_token) {
  if (!thread_.task_runner()->BelongsToCurrentThread()) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ClientGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                       base::Unretained(this), id, sync_token));
    return;
  }

  if (gpu_)
    gpu_->DestroyGpuMemoryBuffer(id, sync_token);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
ClientGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  // Note: this can be called from multiple threads at the same time. Some of
  // those threads may not have a TaskRunner set.
  DCHECK_EQ(gpu::kNullSurfaceHandle, surface_handle);
  CHECK(!thread_.task_runner()->BelongsToCurrentThread());
  gfx::GpuMemoryBufferHandle gmb_handle;
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientGpuMemoryBufferManager::AllocateGpuMemoryBufferOnThread,
          base::Unretained(this), size, format, usage, &gmb_handle, &wait));
  wait.Wait();
  if (gmb_handle.is_null())
    return nullptr;

  auto gmb_handle_id = gmb_handle.id;
  auto callback =
      base::BindOnce(&ClientGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                     weak_ptr_, gmb_handle_id);
  std::unique_ptr<gpu::GpuMemoryBufferImpl> buffer =
      gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
          std::move(gmb_handle), size, format, usage,
          base::BindOnce(&NotifyDestructionOnCorrectThread,
                         thread_.task_runner(), std::move(callback)));
  if (!buffer) {
    DeletedGpuMemoryBuffer(gmb_handle_id, gpu::SyncToken());
    return nullptr;
  }
  return std::move(buffer);
}

void ClientGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

}  // namespace viz
