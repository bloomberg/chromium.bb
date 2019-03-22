// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/shared_image_interface_proxy.h"

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"

namespace gpu {

SharedImageInterfaceProxy::SharedImageInterfaceProxy(GpuChannelHost* host,
                                                     int32_t route_id)
    : host_(host), route_id_(route_id) {}

SharedImageInterfaceProxy::~SharedImageInterfaceProxy() = default;
Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  GpuChannelMsg_CreateSharedImage_Params params;
  params.mailbox = Mailbox::Generate();
  params.format = format;
  params.size = size;
  params.color_space = color_space;
  params.usage = usage;
  {
    base::AutoLock lock(lock_);
    params.release_id = ++next_release_id_;
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_CreateSharedImage(route_id_, params));
  }
  return params.mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  DCHECK(gpu_memory_buffer_manager);
  GpuChannelMsg_CreateGMBSharedImage_Params params;
  params.mailbox = Mailbox::Generate();
  params.handle = gpu_memory_buffer->CloneHandle();
  params.size = gpu_memory_buffer->GetSize();
  params.format = gpu_memory_buffer->GetFormat();
  params.color_space = color_space;
  params.usage = usage;

  // TODO(piman): DCHECK GMB format support.
  DCHECK(gpu::IsImageSizeValidForGpuMemoryBufferFormat(params.size,
                                                       params.format));

  bool requires_sync_token = params.handle.type == gfx::IO_SURFACE_BUFFER;
  {
    base::AutoLock lock(lock_);
    params.release_id = ++next_release_id_;
    // Note: we send the IPC under the lock, after flushing previous work (if
    // any) to guarantee monotonicity of the release ids as seen by the service.
    // Although we don't strictly need to for correctness, we also flush
    // DestroySharedImage messages, so that we get a chance to delete resources
    // before creating new ones.
    // TODO(piman): support messages with handles in EnqueueDeferredMessage.
    host_->EnsureFlush(last_flush_id_);
    host_->Send(
        new GpuChannelMsg_CreateGMBSharedImage(route_id_, std::move(params)));
  }
  if (requires_sync_token) {
    gpu::SyncToken sync_token = GenUnverifiedSyncToken();

    // Force a synchronous IPC to validate sync token.
    host_->VerifyFlush(UINT32_MAX);
    sync_token.SetVerifyFlush();

    gpu_memory_buffer_manager->SetDestructionSyncToken(gpu_memory_buffer,
                                                       sync_token);
  }
  return params.mailbox;
}

void SharedImageInterfaceProxy::UpdateSharedImage(const SyncToken& sync_token,
                                                  const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host_->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_UpdateSharedImage(route_id_, mailbox, ++next_release_id_),
        std::move(dependencies));
  }
}

void SharedImageInterfaceProxy::DestroySharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host_->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  uint32_t flush_id = host_->EnqueueDeferredMessage(
      GpuChannelMsg_DestroySharedImage(route_id_, mailbox),
      std::move(dependencies));
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = flush_id;
  }
}

SyncToken SharedImageInterfaceProxy::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      next_release_id_);
}

}  // namespace gpu
