// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include <stddef.h>

#include <new>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

#if defined(OS_CHROMEOS)
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gl/gl_image_native_pixmap.h"
#endif

namespace gpu {
class Buffer;

#if defined(OS_CHROMEOS)
namespace {

struct CleanUpContext {
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;
  SharedContextState* shared_context_state = nullptr;
  scoped_refptr<gl::GLImage> gl_image;
  GLuint texture = 0;
};

void CleanUpResource(SkImage::ReleaseContext context) {
  auto* clean_up_context = static_cast<CleanUpContext*>(context);
  DCHECK(clean_up_context->main_task_runner->BelongsToCurrentThread());
  if (clean_up_context->shared_context_state->IsCurrent(
          nullptr /* surface */)) {
    DCHECK(!clean_up_context->shared_context_state->context_lost());
    glDeleteTextures(1u, &clean_up_context->texture);
  } else {
    DCHECK(clean_up_context->shared_context_state->context_lost());
  }
  // The GLImage is destroyed here (it should be destroyed regardless of whether
  // the context is lost or current).
  delete clean_up_context;
}

}  // namespace
#endif

ImageDecodeAcceleratorStub::ImageDecodeAcceleratorStub(
    ImageDecodeAcceleratorWorker* worker,
    GpuChannel* channel,
    int32_t route_id)
    : worker_(worker),
      channel_(channel),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel->client_id(),
                                                 route_id),
              sequence_)),
      main_task_runner_(channel->task_runner()),
      io_task_runner_(channel->io_task_runner()) {
  // We need the sequence to be initially disabled so that when we schedule a
  // task to release the decode sync token, it doesn't run immediately (we want
  // it to run when the decode is done).
  channel_->scheduler()->DisableSequence(sequence_);
}

bool ImageDecodeAcceleratorStub::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration)) {
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageDecodeAcceleratorStub, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_ScheduleImageDecode,
                        OnScheduleImageDecode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageDecodeAcceleratorStub::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  sync_point_client_state_->Destroy();
  channel_->scheduler()->DestroySequence(sequence_);
  channel_ = nullptr;
}

void ImageDecodeAcceleratorStub::SetImageFactoryForTesting(
    ImageFactory* image_factory) {
  external_image_factory_for_testing_ = image_factory;
}

ImageDecodeAcceleratorStub::~ImageDecodeAcceleratorStub() {
  DCHECK(!channel_);
}

void ImageDecodeAcceleratorStub::OnScheduleImageDecode(
    const GpuChannelMsg_ScheduleImageDecode_Params& decode_params,
    uint64_t release_count) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_ || destroying_channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  // Make sure the decode sync token is ordered with respect to the last decode
  // request.
  if (release_count <= last_release_count_) {
    DLOG(ERROR) << "Out-of-order decode sync token";
    OnError();
    return;
  }
  last_release_count_ = release_count;

  // Make sure the output dimensions are not too small.
  if (decode_params.output_size.IsEmpty()) {
    DLOG(ERROR) << "Output dimensions are too small";
    OnError();
    return;
  }

  // TODO(andrescj): for now, reject requests that need mipmaps until we support
  // generating mipmap chains.
  if (decode_params.needs_mips) {
    DLOG(ERROR) << "Generating mipmaps is not supported";
    OnError();
    return;
  }

  // Start the actual decode.
  worker_->Decode(
      std::move(decode_params.encoded_data), decode_params.output_size,
      base::BindOnce(&ImageDecodeAcceleratorStub::OnDecodeCompleted,
                     base::WrapRefCounted(this), decode_params.output_size));

  // Schedule a task to eventually release the decode sync token. Note that this
  // task won't run until the sequence is re-enabled when a decode completes.
  const SyncToken discardable_handle_sync_token = SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(channel_->client_id(),
                                         decode_params.raster_decoder_route_id),
      decode_params.discardable_handle_release_count);
  channel_->scheduler()->ScheduleTask(Scheduler::Task(
      sequence_,
      base::BindOnce(&ImageDecodeAcceleratorStub::ProcessCompletedDecode,
                     base::WrapRefCounted(this), std::move(decode_params),
                     release_count),
      {discardable_handle_sync_token} /* sync_token_fences */));
}

void ImageDecodeAcceleratorStub::ProcessCompletedDecode(
    GpuChannelMsg_ScheduleImageDecode_Params params,
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_ || destroying_channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  DCHECK(!pending_completed_decodes_.empty());
  std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> completed_decode =
      std::move(pending_completed_decodes_.front());

  // Gain access to the transfer cache through the GpuChannelManager's
  // SharedContextState. We will also use that to get a GrContext that will be
  // used for Skia operations.
  ContextResult context_result;
  scoped_refptr<SharedContextState> shared_context_state =
      channel_->gpu_channel_manager()->GetSharedContextState(&context_result);
  if (context_result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Unable to obtain the SharedContextState";
    OnError();
    return;
  }
  DCHECK(shared_context_state);

  // TODO(andrescj): in addition to this check, we should not advertise support
  // for hardware decode acceleration if we're not using GL (until we support
  // other graphics APIs).
  if (!shared_context_state->IsGLInitialized()) {
    DLOG(ERROR) << "GL has not been initialized";
    OnError();
    return;
  }
  if (!shared_context_state->gr_context()) {
    DLOG(ERROR) << "Could not get the GrContext";
    OnError();
    return;
  }
  if (!shared_context_state->MakeCurrent(nullptr /* surface */)) {
    DLOG(ERROR) << "Could not MakeCurrent the shared context";
    OnError();
    return;
  }

  std::vector<sk_sp<SkImage>> plane_sk_images;
#if defined(OS_CHROMEOS)
  // Right now, we only support YUV 4:2:0 for the output of the decoder.
  //
  // TODO(andrescj): change to gfx::BufferFormat::YUV_420 once
  // https://crrev.com/c/1573718 lands.
  DCHECK_EQ(gfx::BufferFormat::YVU_420, completed_decode->buffer_format);
  DCHECK_EQ(3u, completed_decode->handle.native_pixmap_handle.planes.size());

  // Calculate the dimensions of each of the planes.
  const gfx::Size y_plane_size = completed_decode->visible_size;
  base::CheckedNumeric<int> safe_uv_width(y_plane_size.width());
  base::CheckedNumeric<int> safe_uv_height(y_plane_size.height());
  safe_uv_width += 1;
  safe_uv_width /= 2;
  safe_uv_height += 1;
  safe_uv_height /= 2;
  int uv_width;
  int uv_height;
  if (!safe_uv_width.AssignIfValid(&uv_width) ||
      !safe_uv_height.AssignIfValid(&uv_height)) {
    DLOG(ERROR) << "Could not calculate subsampled dimensions";
    OnError();
    return;
  }
  gfx::Size uv_plane_size = gfx::Size(uv_width, uv_height);

  // Create a gl::GLImage for each plane and attach it to a texture.
  plane_sk_images.resize(3u);
  for (size_t plane = 0u; plane < 3u; plane++) {
    // |resource_cleaner| will be called to delete textures and GLImages that we
    // create in this section in case of an early return.
    CleanUpContext* resource = new CleanUpContext{};
    resource->main_task_runner = channel_->task_runner();
    resource->shared_context_state = shared_context_state.get();
    // The use of base::Unretained() is safe because the |resource| is allocated
    // using new and is deleted inside CleanUpResource().
    base::ScopedClosureRunner resource_cleaner(
        base::BindOnce(&CleanUpResource, base::Unretained(resource)));
    glGenTextures(1u, &resource->texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, resource->texture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gfx::Size plane_size = plane == 0 ? y_plane_size : uv_plane_size;

    // Extract the plane out of |completed_decode->handle| and put it in its own
    // gfx::GpuMemoryBufferHandle so that we can create an R_8 image for the
    // plane.
    gfx::GpuMemoryBufferHandle plane_handle;
    plane_handle.type = completed_decode->handle.type;
    plane_handle.native_pixmap_handle.planes.push_back(
        std::move(completed_decode->handle.native_pixmap_handle.planes[plane]));
    scoped_refptr<gl::GLImage> plane_image;
    if (external_image_factory_for_testing_) {
      plane_image =
          external_image_factory_for_testing_->CreateImageForGpuMemoryBuffer(
              std::move(plane_handle), plane_size, gfx::BufferFormat::R_8,
              -1 /* client_id */, kNullSurfaceHandle);
    } else {
      auto plane_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
          plane_size, gfx::BufferFormat::R_8,
          std::move(plane_handle.native_pixmap_handle));
      auto plane_image_native_pixmap =
          base::MakeRefCounted<gl::GLImageNativePixmap>(plane_size,
                                                        gfx::BufferFormat::R_8);
      if (plane_image_native_pixmap->Initialize(plane_pixmap))
        plane_image = std::move(plane_image_native_pixmap);
    }
    if (!plane_image) {
      DLOG(ERROR) << "Could not create GL image";
      OnError();
      return;
    }
    resource->gl_image = std::move(plane_image);
    if (!resource->gl_image->BindTexImage(GL_TEXTURE_EXTERNAL_OES)) {
      DLOG(ERROR) << "Could not bind GL image to texture";
      OnError();
      return;
    }

    // Create a SkImage using the texture.
    const GrBackendTexture plane_backend_texture(
        plane_size.width(), plane_size.height(), GrMipMapped::kNo,
        GrGLTextureInfo{GL_TEXTURE_EXTERNAL_OES, resource->texture, GL_R8_EXT});
    plane_sk_images[plane] = SkImage::MakeFromTexture(
        shared_context_state->gr_context(), plane_backend_texture,
        kTopLeft_GrSurfaceOrigin, kGray_8_SkColorType, kOpaque_SkAlphaType,
        nullptr /* colorSpace */, CleanUpResource, resource);
    if (!plane_sk_images[plane]) {
      DLOG(ERROR) << "Could not create planar SkImage";
      OnError();
      return;
    }
    // No need for us to call the resource cleaner. Skia should do that.
    resource_cleaner.Release().Reset();
  }
#else
  // Right now, we only support Chrome OS because we need to use the
  // |native_pixmap_handle| member of a GpuMemoryBufferHandle.
  NOTIMPLEMENTED()
      << "Image decode acceleration is unsupported for this platform";
  OnError();
  return;
#endif

  // Insert the cache entry in the transfer cache. Note that this section
  // validates several of the IPC parameters: |params.raster_decoder_route_id|,
  // |params.transfer_cache_entry_id|, |params.discardable_handle_shm_id|, and
  // |params.discardable_handle_shm_offset|.
  CommandBufferStub* command_buffer =
      channel_->LookupCommandBuffer(params.raster_decoder_route_id);
  if (!command_buffer) {
    DLOG(ERROR) << "Could not find the command buffer";
    OnError();
    return;
  }
  scoped_refptr<Buffer> handle_buffer =
      command_buffer->GetTransferBuffer(params.discardable_handle_shm_id);
  if (!DiscardableHandleBase::ValidateParameters(
          handle_buffer.get(), params.discardable_handle_shm_offset)) {
    DLOG(ERROR) << "Could not validate the discardable handle parameters";
    OnError();
    return;
  }
  DCHECK(command_buffer->decoder_context());
  if (command_buffer->decoder_context()->GetRasterDecoderId() < 0) {
    DLOG(ERROR) << "Could not get the raster decoder ID";
    OnError();
    return;
  }
  DCHECK(shared_context_state->transfer_cache());
  if (!shared_context_state->transfer_cache()
           ->CreateLockedHardwareDecodedImageEntry(
               command_buffer->decoder_context()->GetRasterDecoderId(),
               params.transfer_cache_entry_id,
               ServiceDiscardableHandle(std::move(handle_buffer),
                                        params.discardable_handle_shm_offset,
                                        params.discardable_handle_shm_id),
               shared_context_state->gr_context(), std::move(plane_sk_images),
               completed_decode->buffer_byte_size, params.needs_mips,
               params.target_color_space.ToSkColorSpace())) {
    DLOG(ERROR) << "Could not create and insert the transfer cache entry";
    OnError();
    return;
  }
  shared_context_state->set_need_context_state_reset(true);

  // All done! The decoded image can now be used for rasterization, so we can
  // release the decode sync token.
  sync_point_client_state_->ReleaseFenceSync(decode_release_count);

  // If there are no more completed decodes to be processed, we can disable the
  // sequence: when the next decode is completed, the sequence will be
  // re-enabled.
  pending_completed_decodes_.pop();
  if (pending_completed_decodes_.empty())
    channel_->scheduler()->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnDecodeCompleted(
    gfx::Size expected_output_size,
    std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> result) {
  base::AutoLock lock(lock_);
  if (!channel_ || destroying_channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  if (!result) {
    DLOG(ERROR) << "The decode failed";
    OnError();
    return;
  }

  // A sanity check on the output of the decoder.
  DCHECK(expected_output_size == result->visible_size);

  // The decode is ready to be processed: add it to |pending_completed_decodes_|
  // so that ProcessCompletedDecode() can pick it up.
  pending_completed_decodes_.push(std::move(result));

  // We only need to enable the sequence when the number of pending completed
  // decodes is 1. If there are more, the sequence should already be enabled.
  if (pending_completed_decodes_.size() == 1u)
    channel_->scheduler()->EnableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnError() {
  lock_.AssertAcquired();
  DCHECK(channel_);

  // Trigger the destruction of the channel and stop processing further
  // completed decodes, even if they're successful. We can't call
  // GpuChannel::OnChannelError() directly because that will end up calling
  // ImageDecodeAcceleratorStub::Shutdown() while |lock_| is still acquired. So,
  // we post a task to the main thread instead.
  destroying_channel_ = true;
  channel_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuChannel::OnChannelError, channel_->AsWeakPtr()));
}

}  // namespace gpu
