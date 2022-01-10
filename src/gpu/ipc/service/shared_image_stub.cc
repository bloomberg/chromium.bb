// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/shared_image_stub.h"

#include <inttypes.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_context.h"

namespace gpu {

SharedImageStub::SharedImageStub(GpuChannel* channel, int32_t route_id)
    : channel_(channel),
      command_buffer_id_(
          CommandBufferIdFromChannelAndRoute(channel->client_id(), route_id)),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                                     channel_->task_runner())),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              command_buffer_id_,
              sequence_)) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "gpu::SharedImageStub", channel_->task_runner());
}

SharedImageStub::~SharedImageStub() {
  channel_->scheduler()->DestroySequence(sequence_);
  sync_point_client_state_->Destroy();
  if (factory_ && factory_->HasImages()) {
    bool have_context = MakeContextCurrent();
    factory_->DestroyAllSharedImages(have_context);
  }
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

std::unique_ptr<SharedImageStub> SharedImageStub::Create(GpuChannel* channel,
                                                         int32_t route_id) {
  auto stub = base::WrapUnique(new SharedImageStub(channel, route_id));
  ContextResult result = stub->MakeContextCurrentAndCreateFactory();
  if (result == ContextResult::kSuccess)
    return stub;

  // If it's not a transient failure, treat it as fatal.
  if (result != ContextResult::kTransientFailure)
    return nullptr;

  // For transient failure, retry once to create a shared context state and
  // hence factory again.
  if (stub->MakeContextCurrentAndCreateFactory() != ContextResult::kSuccess)
    return nullptr;
  return stub;
}

void SharedImageStub::ExecuteDeferredRequest(
    mojom::DeferredSharedImageRequestPtr request) {
  switch (request->which()) {
    case mojom::DeferredSharedImageRequest::Tag::kNop:
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImage:
      OnCreateSharedImage(std::move(request->get_create_shared_image()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImageWithData:
      OnCreateSharedImageWithData(
          std::move(request->get_create_shared_image_with_data()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCreateGmbSharedImage:
      OnCreateGMBSharedImage(std::move(request->get_create_gmb_shared_image()));
      break;

#if defined(OS_ANDROID)
    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImageWithAhb: {
      auto& create_request = *request->get_create_shared_image_with_ahb();
      OnCreateSharedImageWithAHB(
          create_request.out_mailbox, create_request.in_mailbox,
          create_request.usage, create_request.release_id);
      break;
    }
#endif  // defined(OS_ANDROID)

    case mojom::DeferredSharedImageRequest::Tag::kRegisterUploadBuffer:
      OnRegisterSharedImageUploadBuffer(
          std::move(request->get_register_upload_buffer()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kUpdateSharedImage: {
      auto& update = *request->get_update_shared_image();
      OnUpdateSharedImage(update.mailbox, update.release_id,
                          std::move(update.in_fence_handle));
      break;
    }

    case mojom::DeferredSharedImageRequest::Tag::kDestroySharedImage:
      OnDestroySharedImage(request->get_destroy_shared_image());
      break;

#if defined(OS_WIN)
    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImageVideoPlanes:
      OnCreateSharedImageVideoPlanes(
          std::move(request->get_create_shared_image_video_planes()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCopyToGpuMemoryBuffer: {
      auto& params = *request->get_copy_to_gpu_memory_buffer();
      OnCopyToGpuMemoryBuffer(params.mailbox, params.release_id);
      break;
    }

    case mojom::DeferredSharedImageRequest::Tag::kCreateSwapChain:
      OnCreateSwapChain(std::move(request->get_create_swap_chain()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kPresentSwapChain:
      OnPresentSwapChain(request->get_present_swap_chain()->mailbox,
                         request->get_present_swap_chain()->release_id);
      break;
#endif  // defined(OS_WIN)
  }
}

bool SharedImageStub::CreateSharedImage(const Mailbox& mailbox,
                                        int client_id,
                                        gfx::GpuMemoryBufferHandle handle,
                                        gfx::BufferFormat format,
                                        gfx::BufferPlane plane,
                                        SurfaceHandle surface_handle,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        uint32_t usage) {
  TRACE_EVENT2("gpu", "SharedImageStub::CreateSharedImage", "width",
               size.width(), "height", size.height());
  if (!mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageStub: Trying to create a SharedImage with a "
                  "non-SharedImage mailbox.";
    OnError();
    return false;
  }
  if (!MakeContextCurrent()) {
    OnError();
    return false;
  }
  if (!factory_->CreateSharedImage(
          mailbox, client_id, std::move(handle), format, plane, surface_handle,
          size, color_space, surface_origin, alpha_type, usage)) {
    LOG(ERROR) << "SharedImageStub: Unable to create shared image";
    OnError();
    return false;
  }
  return true;
}

bool SharedImageStub::UpdateSharedImage(const Mailbox& mailbox,
                                        gfx::GpuFenceHandle in_fence_handle) {
  TRACE_EVENT0("gpu", "SharedImageStub::UpdateSharedImage");
  std::unique_ptr<gfx::GpuFence> in_fence;
  if (!in_fence_handle.is_null())
    in_fence = std::make_unique<gfx::GpuFence>(std::move(in_fence_handle));
  if (!mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageStub: Trying to access a SharedImage with a "
                  "non-SharedImage mailbox.";
    OnError();
    return false;
  }
  if (!MakeContextCurrent()) {
    OnError();
    return false;
  }
  if (!factory_->UpdateSharedImage(mailbox, std::move(in_fence))) {
    LOG(ERROR) << "SharedImageStub: Unable to update shared image";
    OnError();
    return false;
  }
  return true;
}

#if defined(OS_ANDROID)
bool SharedImageStub::CreateSharedImageWithAHB(const Mailbox& out_mailbox,
                                               const Mailbox& in_mailbox,
                                               uint32_t usage) {
  TRACE_EVENT0("gpu", "SharedImageStub::CreateSharedImageWithAHB");
  if (!out_mailbox.IsSharedImage() || !in_mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageStub: Trying to access a SharedImage with a "
                  "non-SharedImage mailbox.";
    OnError();
    return false;
  }
  if (!MakeContextCurrent()) {
    OnError();
    return false;
  }
  if (!factory_->CreateSharedImageWithAHB(out_mailbox, in_mailbox, usage)) {
    LOG(ERROR) << "SharedImageStub: Unable to update shared image";
    return false;
  }
  return true;
}
#endif

void SharedImageStub::OnCreateSharedImage(
    mojom::CreateSharedImageParamsPtr params) {
  TRACE_EVENT2("gpu", "SharedImageStub::OnCreateSharedImage", "width",
               params->size.width(), "height", params->size.height());
  if (!params->mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageStub: Trying to create a SharedImage with a "
                  "non-SharedImage mailbox.";
    OnError();
    return;
  }

  // Some shared image backing factories will use GL.
  // TODO(crbug.com/1239365): Only request GL when needed.
  if (!MakeContextCurrent(/*needs_gl=*/true)) {
    OnError();
    return;
  }

  if (!factory_->CreateSharedImage(params->mailbox, params->format,
                                   params->size, params->color_space,
                                   params->surface_origin, params->alpha_type,
                                   gpu::kNullSurfaceHandle, params->usage)) {
    LOG(ERROR) << "SharedImageStub: Unable to create shared image";
    OnError();
    return;
  }

  sync_point_client_state_->ReleaseFenceSync(params->release_id);
}

void SharedImageStub::OnCreateSharedImageWithData(
    mojom::CreateSharedImageWithDataParamsPtr params) {
  TRACE_EVENT2("gpu", "SharedImageStub::OnCreateSharedImageWithData", "width",
               params->size.width(), "height", params->size.height());
  if (!params->mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageStub: Trying to create a SharedImage with a "
                  "non-SharedImage mailbox.";
    OnError();
    return;
  }

  if (!MakeContextCurrent()) {
    OnError();
    return;
  }

  base::CheckedNumeric<size_t> safe_required_span_size =
      params->pixel_data_offset;
  safe_required_span_size += params->pixel_data_size;
  size_t required_span_size;
  if (!safe_required_span_size.AssignIfValid(&required_span_size)) {
    LOG(ERROR) << "SharedImageStub: upload data size and offset is invalid";
    OnError();
    return;
  }

  auto memory =
      upload_memory_mapping_.GetMemoryAsSpan<uint8_t>(required_span_size);
  if (memory.empty()) {
    LOG(ERROR) << "SharedImageStub: upload data does not have expected size";
    OnError();
    return;
  }

  auto subspan =
      memory.subspan(params->pixel_data_offset, params->pixel_data_size);

  if (!factory_->CreateSharedImage(
          params->mailbox, params->format, params->size, params->color_space,
          params->surface_origin, params->alpha_type, params->usage, subspan)) {
    LOG(ERROR) << "SharedImageStub: Unable to create shared image";
    OnError();
    return;
  }

  // If this is the last upload using a given buffer, release it.
  if (params->done_with_shm) {
    upload_memory_mapping_ = base::ReadOnlySharedMemoryMapping();
    upload_memory_ = base::ReadOnlySharedMemoryRegion();
  }

  sync_point_client_state_->ReleaseFenceSync(params->release_id);
}

void SharedImageStub::OnCreateGMBSharedImage(
    mojom::CreateGMBSharedImageParamsPtr params) {
  TRACE_EVENT2("gpu", "SharedImageStub::OnCreateGMBSharedImage", "width",
               params->size.width(), "height", params->size.height());
  // TODO(piman): add support for SurfaceHandle (for backbuffers for ozone/drm).
  constexpr SurfaceHandle surface_handle = kNullSurfaceHandle;
  if (!CreateSharedImage(params->mailbox, channel_->client_id(),
                         std::move(params->buffer_handle), params->format,
                         params->plane, surface_handle, params->size,
                         params->color_space, params->surface_origin,
                         params->alpha_type, params->usage)) {
    return;
  }

  sync_point_client_state_->ReleaseFenceSync(params->release_id);
}

void SharedImageStub::OnUpdateSharedImage(const Mailbox& mailbox,
                                          uint32_t release_id,
                                          gfx::GpuFenceHandle in_fence_handle) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnUpdateSharedImage");

  if (!UpdateSharedImage(mailbox, std::move(in_fence_handle)))
    return;

  sync_point_client_state_->ReleaseFenceSync(release_id);
}

#if defined(OS_ANDROID)
void SharedImageStub::OnCreateSharedImageWithAHB(const Mailbox& out_mailbox,
                                                 const Mailbox& in_mailbox,
                                                 uint32_t usage,
                                                 uint32_t release_id) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnCreateSharedImageWithAHB");

  if (!CreateSharedImageWithAHB(out_mailbox, in_mailbox, usage))
    return;

  sync_point_client_state_->ReleaseFenceSync(release_id);
}
#endif

void SharedImageStub::OnDestroySharedImage(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnDestroySharedImage");
  if (!mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageStub: Trying to destroy a SharedImage with a "
                  "non-SharedImage mailbox.";
    OnError();
    return;
  }

  if (!MakeContextCurrent()) {
    OnError();
    return;
  }

  if (!factory_->DestroySharedImage(mailbox)) {
    LOG(ERROR) << "SharedImageStub: Unable to destroy shared image";
    OnError();
    return;
  }
}

#if defined(OS_WIN)
void SharedImageStub::OnCreateSharedImageVideoPlanes(
    mojom::CreateSharedImageVideoPlanesParamsPtr params) {
  TRACE_EVENT0("gpu", "SharedImageStub::CreateSharedImageVideoPlanes");
  for (const auto& mailbox : params->mailboxes) {
    if (!mailbox.IsSharedImage()) {
      DLOG(ERROR) << "SharedImageStub: Trying to create a SharedImage video "
                     "plane with a non-SharedImage mailbox.";
      OnError();
      return;
    }
  }
  if (!MakeContextCurrent()) {
    OnError();
    return;
  }
  if (!factory_->CreateSharedImageVideoPlanes(
          std::move(params->mailboxes), std::move(params->gmb_handle),
          params->format, params->size, params->usage)) {
    DLOG(ERROR)
        << "SharedImageStub: Failed to create shared image video planes";
    OnError();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(params->release_id);
}

void SharedImageStub::OnCopyToGpuMemoryBuffer(const Mailbox& mailbox,
                                              uint32_t release_id) {
  if (!mailbox.IsSharedImage()) {
    DLOG(ERROR) << "SharedImageStub: Trying to access a SharedImage with a "
                   "non-SharedImage mailbox.";
    OnError();
    return;
  }
  if (!MakeContextCurrent()) {
    OnError();
    return;
  }
  if (!factory_->CopyToGpuMemoryBuffer(mailbox)) {
    DLOG(ERROR) << "SharedImageStub: Unable to update shared GMB";
    OnError();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(release_id);
}

void SharedImageStub::OnCreateSwapChain(
    mojom::CreateSwapChainParamsPtr params) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnCreateSwapChain");

  if (!params->front_buffer_mailbox.IsSharedImage() ||
      !params->back_buffer_mailbox.IsSharedImage()) {
    DLOG(ERROR) << "SharedImageStub: Trying to access SharedImage with a "
                   "non-SharedImage mailbox.";
    OnError();
    return;
  }

  if (!MakeContextCurrent()) {
    OnError();
    return;
  }

  if (!factory_->CreateSwapChain(
          params->front_buffer_mailbox, params->back_buffer_mailbox,
          params->format, params->size, params->color_space,
          params->surface_origin, params->alpha_type, params->usage)) {
    DLOG(ERROR) << "SharedImageStub: Unable to create swap chain";
    OnError();
    return;
  }

  sync_point_client_state_->ReleaseFenceSync(params->release_id);
}

void SharedImageStub::OnPresentSwapChain(const Mailbox& mailbox,
                                         uint32_t release_id) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnPresentSwapChain");

  if (!mailbox.IsSharedImage()) {
    DLOG(ERROR) << "SharedImageStub: Trying to access a SharedImage with a "
                   "non-SharedImage mailbox.";
    OnError();
    return;
  }

  if (!MakeContextCurrent()) {
    OnError();
    return;
  }

  if (!factory_->PresentSwapChain(mailbox)) {
    DLOG(ERROR) << "SharedImageStub: Unable to present swap chain";
    OnError();
    return;
  }

  sync_point_client_state_->ReleaseFenceSync(release_id);
}
#endif  // OS_WIN

#if defined(OS_FUCHSIA)
void SharedImageStub::RegisterSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  if (!id || !token) {
    OnError();
    return;
  }

  if (!factory_->RegisterSysmemBufferCollection(
          id, std::move(token), format, usage, register_with_image_pipe)) {
    OnError();
  }
}

void SharedImageStub::ReleaseSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id) {
  if (!factory_->ReleaseSysmemBufferCollection(id)) {
    DLOG(ERROR) << "SharedImageStub: Trying to release unknown "
                   "SysmemBufferCollectionId.";
    OnError();
    return;
  }
}
#endif  // defined(OS_FUCHSIA)

void SharedImageStub::OnRegisterSharedImageUploadBuffer(
    base::ReadOnlySharedMemoryRegion shm) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnRegisterSharedImageUploadBuffer");
  upload_memory_ = std::move(shm);
  upload_memory_mapping_ = upload_memory_.Map();
  if (!upload_memory_mapping_.IsValid()) {
    LOG(ERROR)
        << "SharedImageStub: Unable to map shared memory for upload data";
    OnError();
    return;
  }
}

bool SharedImageStub::MakeContextCurrent(bool needs_gl) {
  DCHECK(context_state_);

  if (context_state_->context_lost()) {
    LOG(ERROR) << "SharedImageStub: context already lost";
    return false;
  }

  // |factory_| never writes to the surface, so pass nullptr to
  // improve performance. https://crbug.com/457431
  auto* context = context_state_->real_context();
  if (context->IsCurrent(nullptr))
    return !context_state_->CheckResetStatus(needs_gl);
  return context_state_->MakeCurrent(/*surface=*/nullptr, needs_gl);
}

ContextResult SharedImageStub::MakeContextCurrentAndCreateFactory() {
  auto* channel_manager = channel_->gpu_channel_manager();
  DCHECK(!context_state_);
  ContextResult result;
  context_state_ = channel_manager->GetSharedContextState(&result);
  if (result != ContextResult::kSuccess) {
    LOG(ERROR) << "SharedImageStub: unable to create context";
    context_state_ = nullptr;
    return result;
  }
  DCHECK(context_state_);
  DCHECK(!context_state_->context_lost());
  // Some shared image backing factories will use GL in ctor, so we need GL even
  // if chrome is using non-GL backing.
  if (!MakeContextCurrent(/*needs_gl=*/true)) {
    context_state_ = nullptr;
    return ContextResult::kTransientFailure;
  }

  gpu::GpuMemoryBufferFactory* gmb_factory =
      channel_manager->gpu_memory_buffer_factory();
  factory_ = std::make_unique<SharedImageFactory>(
      channel_manager->gpu_preferences(),
      channel_manager->gpu_driver_bug_workarounds(),
      channel_manager->gpu_feature_info(), context_state_.get(),
      channel_manager->mailbox_manager(),
      channel_manager->shared_image_manager(),
      gmb_factory ? gmb_factory->AsImageFactory() : nullptr, this,
      features::IsUsingSkiaRenderer(),
      /*is_for_display_compositor=*/false);
  return ContextResult::kSuccess;
}

void SharedImageStub::OnError() {
  channel_->OnChannelError();
}

void SharedImageStub::TrackMemoryAllocatedChange(int64_t delta) {
  DCHECK(delta >= 0 || size_ >= static_cast<uint64_t>(-delta));
  uint64_t old_size = size_;
  size_ += delta;
  channel_->gpu_channel_manager()
      ->peak_memory_monitor()
      ->OnMemoryAllocatedChange(
          command_buffer_id_, old_size, size_,
          GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB);
}

uint64_t SharedImageStub::GetSize() const {
  return size_;
}

uint64_t SharedImageStub::ClientTracingId() const {
  return channel_->client_tracing_id();
}

int SharedImageStub::ClientId() const {
  return channel_->client_id();
}

uint64_t SharedImageStub::ContextGroupTracingId() const {
  return sync_point_client_state_->command_buffer_id().GetUnsafeValue();
}

bool SharedImageStub::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!factory_)
    return true;

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name =
        base::StringPrintf("gpu/shared_images/client_0x%" PRIX32, ClientId());
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes, size_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  return factory_->OnMemoryDump(args, pmd, ClientId(), ClientTracingId());
}

SharedImageStub::SharedImageDestructionCallback
SharedImageStub::GetSharedImageDestructionCallback(const Mailbox& mailbox) {
  return base::BindOnce(&SharedImageStub::DestroySharedImage,
                        weak_factory_.GetWeakPtr(), mailbox);
}

void SharedImageStub::DestroySharedImage(const Mailbox& mailbox,
                                         const SyncToken& sync_token) {
  // If there is no sync token, we don't need to wait.
  if (!sync_token.HasData()) {
    OnDestroySharedImage(mailbox);
    return;
  }

  auto done_cb = base::BindOnce(&SharedImageStub::OnDestroySharedImage,
                                weak_factory_.GetWeakPtr(), mailbox);
  channel_->scheduler()->ScheduleTask(
      gpu::Scheduler::Task(sequence_, std::move(done_cb),
                           std::vector<gpu::SyncToken>({sync_token})));
}

}  // namespace gpu
