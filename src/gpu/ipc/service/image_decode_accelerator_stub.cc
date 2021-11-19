// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include <stddef.h>

#include <algorithm>
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
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gl/gl_image_native_pixmap.h"
#endif

namespace gpu {
class Buffer;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                                     channel->task_runner())),
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

void ImageDecodeAcceleratorStub::ScheduleImageDecode(
    mojom::ScheduleImageDecodeParamsPtr params,
    uint64_t release_count) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration) &&
      !base::FeatureList::IsEnabled(
          features::kVaapiWebPImageDecodeAcceleration)) {
    return;
  }

  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  mojom::ScheduleImageDecodeParams& decode_params = *params;

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
                     base::WrapRefCounted(this), std::move(params),
                     release_count),
      {discardable_handle_sync_token} /* sync_token_fences */));
}

void ImageDecodeAcceleratorStub::ProcessCompletedDecode(
    mojom::ScheduleImageDecodeParamsPtr params_ptr,
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  mojom::ScheduleImageDecodeParams& params = *params_ptr;

  DCHECK(!pending_completed_decodes_.empty());
  std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> completed_decode =
      std::move(pending_completed_decodes_.front());
  pending_completed_decodes_.pop();

  // Regardless of what happens next, make sure the sync token gets released and
  // the sequence gets disabled if there are no more completed decodes after
  // this. base::Unretained(this) is safe because *this outlives the
  // ScopedClosureRunner.
  base::ScopedClosureRunner finalizer(
      base::BindOnce(&ImageDecodeAcceleratorStub::FinishCompletedDecode,
                     base::Unretained(this), decode_release_count));

  if (!completed_decode) {
    DLOG(ERROR) << "The image could not be decoded";
    return;
  }

  // TODO(crbug.com/995883): the output_size parameter is going away, so this
  // validation is not needed. Checking if the size is too small should happen
  // at the level of the decoder (since that's the component that's aware of its
  // own capabilities).
  if (params.output_size.IsEmpty()) {
    DLOG(ERROR) << "Output dimensions are too small";
    return;
  }

  // Gain access to the transfer cache through the GpuChannelManager's
  // SharedContextState. We will also use that to get a GrContext that will be
  // used for Skia operations.
  ContextResult context_result;
  scoped_refptr<SharedContextState> shared_context_state =
      channel_->gpu_channel_manager()->GetSharedContextState(&context_result);
  if (context_result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Unable to obtain the SharedContextState";
    return;
  }
  DCHECK(shared_context_state);

  // TODO(andrescj): in addition to this check, we should not advertise support
  // for hardware decode acceleration if we're not using GL (until we support
  // other graphics APIs).
  if (!shared_context_state->IsGLInitialized()) {
    DLOG(ERROR) << "GL has not been initialized";
    return;
  }
  if (!shared_context_state->gr_context()) {
    DLOG(ERROR) << "Could not get the GrContext";
    return;
  }
  if (!shared_context_state->MakeCurrent(nullptr /* surface */)) {
    DLOG(ERROR) << "Could not MakeCurrent the shared context";
    return;
  }

  std::vector<sk_sp<SkImage>> plane_sk_images;
  absl::optional<base::ScopedClosureRunner> notify_gl_state_changed;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Right now, we only support YUV 4:2:0 for the output of the decoder (either
  // as YV12 or NV12).
  //
  // TODO(andrescj): change to gfx::BufferFormat::YUV_420 once
  // https://crrev.com/c/1573718 lands.
  DCHECK(completed_decode->buffer_format == gfx::BufferFormat::YVU_420 ||
         completed_decode->buffer_format ==
             gfx::BufferFormat::YUV_420_BIPLANAR);
  DCHECK_EQ(
      gfx::NumberOfPlanesForLinearBufferFormat(completed_decode->buffer_format),
      completed_decode->handle.native_pixmap_handle.planes.size());

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
    return;
  }
  gfx::Size uv_plane_size = gfx::Size(uv_width, uv_height);

  // We should notify the SharedContextState that we or Skia may have modified
  // the driver's GL state. We should also notify Skia that we may have modified
  // the graphics API state outside of Skia. We put this in a
  // ScopedClosureRunner so that if we return early, both the SharedContextState
  // and Skia end up in a consistent state.
  notify_gl_state_changed.emplace(base::BindOnce(
      [](scoped_refptr<SharedContextState> scs) {
        scs->set_need_context_state_reset(true);
        scs->PessimisticallyResetGrContext();
      },
      shared_context_state));

  // Create a gl::GLImage for each plane and attach it to a texture.
  const size_t num_planes =
      completed_decode->handle.native_pixmap_handle.planes.size();
  plane_sk_images.resize(num_planes);
  for (size_t plane = 0u; plane < num_planes; plane++) {
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
    // gfx::GpuMemoryBufferHandle so that we can create a GL image for the
    // plane.
    gfx::GpuMemoryBufferHandle plane_handle;
    plane_handle.type = completed_decode->handle.type;
    plane_handle.native_pixmap_handle.planes.push_back(
        std::move(completed_decode->handle.native_pixmap_handle.planes[plane]));
    // Note that the buffer format for the plane is R_8 for all planes if the
    // result of the decode is in YV12. For NV12, the first plane (luma) is R_8
    // and the second plane (chroma) is RG_88.
    const bool is_nv12_chroma_plane = completed_decode->buffer_format ==
                                          gfx::BufferFormat::YUV_420_BIPLANAR &&
                                      plane == 1u;
    const auto plane_format = is_nv12_chroma_plane ? gfx::BufferFormat::RG_88
                                                   : gfx::BufferFormat::R_8;
    scoped_refptr<gl::GLImage> plane_image;
    if (external_image_factory_for_testing_) {
      plane_image =
          external_image_factory_for_testing_->CreateImageForGpuMemoryBuffer(
              std::move(plane_handle), plane_size, plane_format,
              gfx::BufferPlane::DEFAULT, -1 /* client_id */,
              kNullSurfaceHandle);
    } else {
      auto plane_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
          plane_size, plane_format,
          std::move(plane_handle.native_pixmap_handle));
      auto plane_image_native_pixmap =
          base::MakeRefCounted<gl::GLImageNativePixmap>(plane_size,
                                                        plane_format);
      if (plane_image_native_pixmap->Initialize(plane_pixmap))
        plane_image = std::move(plane_image_native_pixmap);
    }
    if (!plane_image) {
      DLOG(ERROR) << "Could not create GL image";
      return;
    }
    resource->gl_image = std::move(plane_image);
    if (!resource->gl_image->BindTexImage(GL_TEXTURE_EXTERNAL_OES)) {
      DLOG(ERROR) << "Could not bind GL image to texture";
      return;
    }

    // Notify Skia that we have changed the driver's GL state outside of Skia.
    shared_context_state->PessimisticallyResetGrContext();

    // Create a SkImage using the texture.
    const GrBackendTexture plane_backend_texture(
        plane_size.width(), plane_size.height(), GrMipMapped::kNo,
        GrGLTextureInfo{GL_TEXTURE_EXTERNAL_OES, resource->texture,
                        static_cast<GrGLenum>(
                            is_nv12_chroma_plane ? GL_RG8_EXT : GL_R8_EXT)});
    plane_sk_images[plane] = SkImage::MakeFromTexture(
        shared_context_state->gr_context(), plane_backend_texture,
        kTopLeft_GrSurfaceOrigin,
        is_nv12_chroma_plane ? kR8G8_unorm_SkColorType : kAlpha_8_SkColorType,
        kOpaque_SkAlphaType, nullptr /* colorSpace */, CleanUpResource,
        resource);
    if (!plane_sk_images[plane]) {
      DLOG(ERROR) << "Could not create planar SkImage";
      return;
    }
    // No need for us to call the resource cleaner. Skia should do that.
    resource_cleaner.Release().Reset();
  }

  // Insert the cache entry in the transfer cache. Note that this section
  // validates several of the IPC parameters: |params.raster_decoder_route_id|,
  // |params.transfer_cache_entry_id|, |params.discardable_handle_shm_id|, and
  // |params.discardable_handle_shm_offset|.
  CommandBufferStub* command_buffer =
      channel_->LookupCommandBuffer(params.raster_decoder_route_id);
  if (!command_buffer) {
    DLOG(ERROR) << "Could not find the command buffer";
    return;
  }
  scoped_refptr<Buffer> handle_buffer =
      command_buffer->GetTransferBuffer(params.discardable_handle_shm_id);
  if (!DiscardableHandleBase::ValidateParameters(
          handle_buffer.get(), params.discardable_handle_shm_offset)) {
    DLOG(ERROR) << "Could not validate the discardable handle parameters";
    return;
  }
  DCHECK(command_buffer->decoder_context());
  if (command_buffer->decoder_context()->GetRasterDecoderId() < 0) {
    DLOG(ERROR) << "Could not get the raster decoder ID";
    return;
  }

  {
    auto* gr_shader_cache = channel_->gpu_channel_manager()->gr_shader_cache();
    absl::optional<raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache)
      cache_use.emplace(gr_shader_cache,
                        base::strict_cast<int32_t>(channel_->client_id()));
    DCHECK(shared_context_state->transfer_cache());
    SkYUVAInfo::PlaneConfig plane_config =
        completed_decode->buffer_format == gfx::BufferFormat::YVU_420
            ? SkYUVAInfo::PlaneConfig::kY_V_U
            : SkYUVAInfo::PlaneConfig::kY_UV;
    // TODO(andrescj): |params.target_color_space| is not needed because Skia
    // knows where it's drawing, so it can handle color space conversion without
    // us having to specify the target color space. However, we are currently
    // assuming that the color space of the image is sRGB. This means we don't
    // support images with embedded color profiles. We could rename
    // |params.target_color_space| to |params.image_color_space| and we can send
    // the embedded color profile from the renderer using that field.
    if (!shared_context_state->transfer_cache()
             ->CreateLockedHardwareDecodedImageEntry(
                 command_buffer->decoder_context()->GetRasterDecoderId(),
                 params.transfer_cache_entry_id,
                 ServiceDiscardableHandle(std::move(handle_buffer),
                                          params.discardable_handle_shm_offset,
                                          params.discardable_handle_shm_id),
                 shared_context_state->gr_context(), std::move(plane_sk_images),
                 plane_config, SkYUVAInfo::Subsampling::k420,
                 completed_decode->yuv_color_space,
                 completed_decode->buffer_byte_size, params.needs_mips)) {
      DLOG(ERROR) << "Could not create and insert the transfer cache entry";
      return;
    }
  }
  DCHECK(notify_gl_state_changed);
  notify_gl_state_changed->RunAndReset();
#else
  // Right now, we only support Chrome OS because we need to use the
  // |native_pixmap_handle| member of a GpuMemoryBufferHandle.
  NOTIMPLEMENTED()
      << "Image decode acceleration is unsupported for this platform";
#endif
}

void ImageDecodeAcceleratorStub::FinishCompletedDecode(
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  sync_point_client_state_->ReleaseFenceSync(decode_release_count);
  if (pending_completed_decodes_.empty())
    channel_->scheduler()->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnDecodeCompleted(
    gfx::Size expected_output_size,
    std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> result) {
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  // A sanity check on the output of the decoder.
  DCHECK(!result || expected_output_size == result->visible_size);

  // The decode is ready to be processed: add it to |pending_completed_decodes_|
  // so that ProcessCompletedDecode() can pick it up.
  pending_completed_decodes_.push(std::move(result));

  // We only need to enable the sequence when the number of pending completed
  // decodes is 1. If there are more, the sequence should already be enabled.
  if (pending_completed_decodes_.size() == 1u)
    channel_->scheduler()->EnableSequence(sequence_);
}

}  // namespace gpu
