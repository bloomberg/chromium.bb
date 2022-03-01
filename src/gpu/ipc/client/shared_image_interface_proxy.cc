// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/shared_image_interface_proxy.h"

#include "base/bits.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
namespace {

bool SafeIncrementAndAlign(size_t aligned_value,
                           size_t increment,
                           size_t alignment,
                           size_t* result) {
  base::CheckedNumeric<size_t> sum = aligned_value;
  sum += increment;
  // Taken from base::bits::Align.
  // TODO(ericrk): Update base::bits::Align to handle CheckedNumeric.
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  sum = (sum + alignment - 1) & ~(alignment - 1);
  return sum.AssignIfValid(result);
}

size_t GetRemainingSize(const base::MappedReadOnlyRegion& region,
                        size_t offset) {
  if (offset > region.mapping.size())
    return 0;
  return region.mapping.size() - offset;
}

void* GetDataAddress(const base::MappedReadOnlyRegion& region,
                     size_t offset,
                     size_t size) {
  base::CheckedNumeric<size_t> safe_end = offset;
  safe_end += size;
  size_t end;
  if (!safe_end.AssignIfValid(&end) || end > region.mapping.size())
    return nullptr;
  return region.mapping.GetMemoryAs<uint8_t>() + offset;
}

std::vector<SyncToken> GenerateDependenciesFromSyncToken(
    SyncToken sync_token,
    GpuChannelHost* const host) {
  DCHECK(host);
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  return dependencies;
}

}  // namespace

SharedImageInterfaceProxy::SharedImageInterfaceProxy(GpuChannelHost* host,
                                                     int32_t route_id)
    : host_(host), route_id_(route_id) {}

SharedImageInterfaceProxy::~SharedImageInterfaceProxy() = default;

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto params = mojom::CreateSharedImageParams::New();
  params->mailbox = mailbox;
  params->format = format;
  params->size = size;
  params->color_space = color_space;
  params->usage = usage;
  params->surface_origin = surface_origin;
  params->alpha_type = alpha_type;
  {
    base::AutoLock lock(lock_);
    AddMailbox(mailbox, usage);
    params->release_id = ++next_release_id_;
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateSharedImage(
                std::move(params))));
  }

  return mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  // Pixel data's size must fit into a uint32_t to be sent in
  // CreateSharedImageWithDataParams.
  if (!base::IsValueInRangeForNumericType<uint32_t>(pixel_data.size())) {
    LOG(ERROR)
        << "CreateSharedImage: SharedImage upload data overflows uint32_t";
    return Mailbox();
  }

  // Hold the lock for the rest of this function, as we need to ensure that SHM
  // reallocation / registration and the following use of that SHM via deferred
  // message are not interrupted by a SHM allocation on another thread.
  base::AutoLock lock(lock_);

  bool done_with_shm;
  size_t shm_offset;
  if (!GetSHMForPixelData(pixel_data, &shm_offset, &done_with_shm)) {
    LOG(ERROR) << "CreateSharedImage: Could not get SHM for data upload.";
    return Mailbox();
  }

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto params = mojom::CreateSharedImageWithDataParams::New();
  params->mailbox = mailbox;
  params->format = format;
  params->size = size;
  params->color_space = color_space;
  params->usage = usage;
  params->pixel_data_offset = shm_offset;
  params->pixel_data_size = pixel_data.size();
  params->done_with_shm = done_with_shm;
  params->release_id = ++next_release_id_;
  params->surface_origin = surface_origin;
  params->alpha_type = alpha_type;
  last_flush_id_ = host_->EnqueueDeferredMessage(
      mojom::DeferredRequestParams::NewSharedImageRequest(
          mojom::DeferredSharedImageRequest::NewCreateSharedImageWithData(
              std::move(params))));
  AddMailbox(mailbox, usage);
  return mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK(gpu_memory_buffer->GetType() == gfx::NATIVE_PIXMAP ||
         gpu_memory_buffer->GetType() == gfx::ANDROID_HARDWARE_BUFFER ||
         gpu_memory_buffer->GetType() == gfx::DXGI_SHARED_HANDLE ||
         gpu_memory_buffer_manager);

  auto mailbox = Mailbox::GenerateForSharedImage();

  auto params = mojom::CreateGMBSharedImageParams::New();
  params->mailbox = mailbox;
  params->buffer_handle = gpu_memory_buffer->CloneHandle();
  params->size = gpu_memory_buffer->GetSize();
  params->format = gpu_memory_buffer->GetFormat();
  params->plane = plane;
  params->color_space = color_space;
  params->usage = usage;
  params->surface_origin = surface_origin;
  params->alpha_type = alpha_type;

  // TODO(piman): DCHECK GMB format support.
  DCHECK(gpu::IsImageSizeValidForGpuMemoryBufferFormat(params->size,
                                                       params->format));

  bool requires_sync_token =
#if defined(OS_FUCHSIA)
      // Synchronization is not required if the image is being created by
      // FuchsiaVideoDecoder. |gpu_memory_buffer_manager| is nullptr in that
      // case.
      (gpu_memory_buffer_manager &&
       params->buffer_handle.type == gfx::NATIVE_PIXMAP) ||
#endif
      params->buffer_handle.type == gfx::IO_SURFACE_BUFFER;
  {
    base::AutoLock lock(lock_);
    params->release_id = ++next_release_id_;
    // Note: we enqueue and send the IPC under the lock to guarantee
    // monotonicity of the release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateGmbSharedImage(
                std::move(params))));
    host_->EnsureFlush(last_flush_id_);
  }
  if (requires_sync_token) {
    DCHECK(gpu_memory_buffer_manager);

    gpu::SyncToken sync_token = GenVerifiedSyncToken();
    gpu_memory_buffer_manager->SetDestructionSyncToken(gpu_memory_buffer,
                                                       sync_token);
  }

  base::AutoLock lock(lock_);
  AddMailbox(mailbox, usage);
  return mailbox;
}

#if defined(OS_WIN)
std::vector<Mailbox> SharedImageInterfaceProxy::CreateSharedImageVideoPlanes(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    uint32_t usage) {
  DCHECK(gpu_memory_buffer_manager);
  DCHECK(gpu_memory_buffer);
  DCHECK_EQ(gpu_memory_buffer->GetType(),
            gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);
  DCHECK_EQ(gpu_memory_buffer->GetFormat(),
            gfx::BufferFormat::YUV_420_BIPLANAR);

  const size_t num_planes =
      gfx::NumberOfPlanesForLinearBufferFormat(gpu_memory_buffer->GetFormat());
  std::vector<Mailbox> mailboxes(num_planes);
  for (auto& mailbox : mailboxes)
    mailbox = Mailbox::GenerateForSharedImage();

  auto params = mojom::CreateSharedImageVideoPlanesParams::New();
  params->mailboxes = mailboxes;
  params->gmb_handle = gpu_memory_buffer->CloneHandle();
  params->size = gpu_memory_buffer->GetSize();
  params->format = gpu_memory_buffer->GetFormat();
  params->usage = usage;
  {
    base::AutoLock lock(lock_);

    for (const auto& mailbox : mailboxes)
      AddMailbox(mailbox, usage);

    params->release_id = ++next_release_id_;
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateSharedImageVideoPlanes(
                std::move(params))));
  }
  return mailboxes;
}

void SharedImageInterfaceProxy::CopyToGpuMemoryBuffer(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCopyToGpuMemoryBuffer(
                mojom::CopyToGpuMemoryBufferParams::New(mailbox,
                                                        ++next_release_id_))),
        std::move(dependencies));
  }
}
#endif  // OS_WIN

#if defined(OS_ANDROID)
Mailbox SharedImageInterfaceProxy::CreateSharedImageWithAHB(
    const Mailbox& mailbox,
    uint32_t usage,
    const SyncToken& sync_token) {
  auto out_mailbox = Mailbox::GenerateForSharedImage();
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    AddMailbox(out_mailbox, usage);
    gfx::GpuFenceHandle acquire_fence_handle;
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateSharedImageWithAhb(
                mojom::CreateSharedImageWithAHBParams::New(
                    out_mailbox, mailbox, usage, ++next_release_id_))),
        std::move(dependencies));
  }
  return out_mailbox;
}
#endif

void SharedImageInterfaceProxy::UpdateSharedImage(const SyncToken& sync_token,
                                                  const Mailbox& mailbox) {
  UpdateSharedImage(sync_token, nullptr, mailbox);
}

void SharedImageInterfaceProxy::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  // If there is a valid SyncToken, there should not be any GpuFence.
  if (sync_token.HasData())
    DCHECK(!acquire_fence);
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);

    gfx::GpuFenceHandle acquire_fence_handle;
    if (acquire_fence)
      acquire_fence_handle = acquire_fence->GetGpuFenceHandle().Clone();
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewUpdateSharedImage(
                mojom::UpdateSharedImageParams::New(
                    mailbox, ++next_release_id_,
                    std::move(acquire_fence_handle)))),
        std::move(dependencies));
  }
}

void SharedImageInterfaceProxy::DestroySharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);

    DCHECK_NE(mailbox_to_usage_.count(mailbox), 0u);
    mailbox_to_usage_.erase(mailbox);

    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewDestroySharedImage(mailbox)),
        std::move(dependencies));
  }
}

SyncToken SharedImageInterfaceProxy::GenVerifiedSyncToken() {
  SyncToken sync_token = GenUnverifiedSyncToken();
  // Force a synchronous IPC to validate sync token.
  host_->VerifyFlush(UINT32_MAX);
  sync_token.SetVerifyFlush();
  return sync_token;
}

SyncToken SharedImageInterfaceProxy::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      next_release_id_);
}

void SharedImageInterfaceProxy::WaitSyncToken(const SyncToken& sync_token) {
  if (!sync_token.HasData())
    return;

  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewNop(0)),
        std::move(dependencies));
  }
}

void SharedImageInterfaceProxy::Flush() {
  base::AutoLock lock(lock_);
  host_->EnsureFlush(last_flush_id_);
}

bool SharedImageInterfaceProxy::GetSHMForPixelData(
    base::span<const uint8_t> pixel_data,
    size_t* shm_offset,
    bool* done_with_shm) {
  const size_t kUploadBufferSize = 1 * 1024 * 1024;  // 1MB
  *shm_offset = 0;
  *done_with_shm = false;

  lock_.AssertAcquired();
  if (!upload_buffer_.IsValid() ||
      GetRemainingSize(upload_buffer_, upload_buffer_offset_) <
          pixel_data.size()) {
    size_t size_to_alloc = std::max(kUploadBufferSize, pixel_data.size());
    auto shm = base::ReadOnlySharedMemoryRegion::Create(size_to_alloc);
    if (!shm.IsValid())
      return false;

    // Duplicate the buffer for sharing to the GPU process.
    base::ReadOnlySharedMemoryRegion readonly_shm = shm.region.Duplicate();
    if (!readonly_shm.IsValid())
      return false;

    // Share the SHM to the GPU process. In order to ensure that any deferred
    // messages which rely on the previous SHM have a chance to execute before
    // it is replaced, send this message in the deferred queue.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewRegisterUploadBuffer(
                std::move(readonly_shm))));
    host_->EnsureFlush(last_flush_id_);

    upload_buffer_ = std::move(shm);
    upload_buffer_offset_ = 0;
  }

  // We now have an |upload_buffer_| that fits our data.

  void* target =
      GetDataAddress(upload_buffer_, upload_buffer_offset_, pixel_data.size());
  DCHECK(target);
  memcpy(target, pixel_data.data(), pixel_data.size());
  *shm_offset = upload_buffer_offset_;

  // Now that we've successfully used up a portion of our buffer, increase our
  // |upload_buffer_offset_|. If our |upload_buffer_offset_| is at the end (or
  // past the end with rounding), we discard the current buffer. We'll allocate
  // a new buffer the next time we enter this function.
  bool discard_buffer = false;
  if (SafeIncrementAndAlign(upload_buffer_offset_, pixel_data.size(),
                            4 /* alignment */, &upload_buffer_offset_)) {
    discard_buffer =
        GetRemainingSize(upload_buffer_, upload_buffer_offset_) == 0;
  } else {
    discard_buffer = true;
  }

  if (discard_buffer) {
    *done_with_shm = true;
    upload_buffer_ = base::MappedReadOnlyRegion();
    upload_buffer_offset_ = 0;
  }

  return true;
}

SharedImageInterface::SwapChainMailboxes
SharedImageInterfaceProxy::CreateSwapChain(viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           uint32_t usage) {
#if defined(OS_WIN)
  const SharedImageInterface::SwapChainMailboxes mailboxes = {
      Mailbox::GenerateForSharedImage(), Mailbox::GenerateForSharedImage()};
  auto params = mojom::CreateSwapChainParams::New();
  params->front_buffer_mailbox = mailboxes.front_buffer;
  params->back_buffer_mailbox = mailboxes.back_buffer;
  params->format = format;
  params->size = size;
  params->color_space = color_space;
  params->usage = usage;
  params->surface_origin = surface_origin;
  params->alpha_type = alpha_type;
  {
    base::AutoLock lock(lock_);

    AddMailbox(mailboxes.front_buffer, usage);
    AddMailbox(mailboxes.back_buffer, usage);

    params->release_id = ++next_release_id_;
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateSwapChain(
                std::move(params))));
  }
  return mailboxes;
#else
  NOTREACHED();
  return {};
#endif  // OS_WIN
}

void SharedImageInterfaceProxy::PresentSwapChain(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
#if defined(OS_WIN)
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    uint32_t release_id = ++next_release_id_;
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewPresentSwapChain(
                mojom::PresentSwapChainParams::New(mailbox, release_id))),
        std::move(dependencies));
    host_->EnsureFlush(last_flush_id_);
  }
#else
  NOTREACHED();
#endif  // OS_WIN
}

#if defined(OS_FUCHSIA)
void SharedImageInterfaceProxy::RegisterSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  host_->GetGpuChannel().RegisterSysmemBufferCollection(
      id, mojo::PlatformHandle(std::move(token)), format, usage,
      register_with_image_pipe);
}

void SharedImageInterfaceProxy::ReleaseSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id) {
  host_->GetGpuChannel().ReleaseSysmemBufferCollection(id);
}
#endif  // defined(OS_FUCHSIA)

scoped_refptr<gfx::NativePixmap> SharedImageInterfaceProxy::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  // Clients outside of the GPU process cannot obtain the backing NativePixmap
  // for SharedImages.
  return nullptr;
}

void SharedImageInterfaceProxy::AddMailbox(const Mailbox& mailbox,
                                           uint32_t usage) {
  lock_.AssertAcquired();

  DCHECK_EQ(mailbox_to_usage_.count(mailbox), 0u);
  mailbox_to_usage_[mailbox] = usage;
}

uint32_t SharedImageInterfaceProxy::UsageForMailbox(const Mailbox& mailbox) {
  base::AutoLock lock(lock_);

  // The mailbox may have been destroyed if the context on which the shared
  // image was created is deleted.
  auto it = mailbox_to_usage_.find(mailbox);
  if (it == mailbox_to_usage_.end())
    return 0u;
  return it->second;
}

void SharedImageInterfaceProxy::NotifyMailboxAdded(const Mailbox& mailbox,
                                                   uint32_t usage) {
  base::AutoLock lock(lock_);
  AddMailbox(mailbox, usage);
}

}  // namespace gpu
