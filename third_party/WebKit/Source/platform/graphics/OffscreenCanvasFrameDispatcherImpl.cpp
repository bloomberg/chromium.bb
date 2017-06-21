// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"

#include "cc/output/compositor_frame.h"
#include "cc/quads/texture_draw_quad.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/OffscreenCanvasPlaceholder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"
#include "platform/wtf/typed_arrays/Uint8Array.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "skia/ext/texture_handle.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/transform.h"

namespace blink {

enum {
  kMaxPendingCompositorFrames = 2,
};

OffscreenCanvasFrameDispatcherImpl::OffscreenCanvasFrameDispatcherImpl(
    OffscreenCanvasFrameDispatcherClient* client,
    uint32_t client_id,
    uint32_t sink_id,
    int canvas_id,
    int width,
    int height)
    : OffscreenCanvasFrameDispatcher(client),
      frame_sink_id_(cc::FrameSinkId(client_id, sink_id)),
      width_(width),
      height_(height),
      change_size_for_next_commit_(false),
      needs_begin_frame_(false),
      next_resource_id_(1u),
      binding_(this),
      buffer_usage_(gfx::BufferUsage::SCANOUT_CPU_READ_WRITE),
      buffer_format_(gfx::BufferFormat::RGBA_8888),
      placeholder_canvas_id_(canvas_id) {
  if (frame_sink_id_.is_valid()) {
    // Only frameless canvas pass an invalid frame sink id; we don't create
    // mojo channel for this special case.
    current_local_surface_id_ = local_surface_id_allocator_.GenerateId();
    DCHECK(!sink_.is_bound());
    mojom::blink::OffscreenCanvasProviderPtr provider;
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&provider));

    cc::mojom::blink::MojoCompositorFrameSinkClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    provider->CreateCompositorFrameSink(frame_sink_id_, std::move(client),
                                        mojo::MakeRequest(&sink_));
  }
}

OffscreenCanvasFrameDispatcherImpl::~OffscreenCanvasFrameDispatcherImpl() {
}

std::unique_ptr<OffscreenCanvasFrameDispatcherImpl::FrameResource>
OffscreenCanvasFrameDispatcherImpl::createOrRecycleFrameResource() {
  if (recycleable_resource_) {
    recycleable_resource_->spare_lock_ = true;
    return std::move(recycleable_resource_);
  }
  return std::unique_ptr<FrameResource>(new FrameResource());
}

bool OffscreenCanvasFrameDispatcherImpl::SetTransferableResourceToSharedBitmap(
    cc::TransferableResource& resource,
    RefPtr<StaticBitmapImage> image) {
  std::unique_ptr<FrameResource> frame_resource =
      createOrRecycleFrameResource();
  if (!frame_resource->shared_bitmap_) {
    frame_resource->shared_bitmap_ =
        Platform::Current()->AllocateSharedBitmap(IntSize(width_, height_));
    if (!frame_resource->shared_bitmap_) {
      LOG(ERROR) << "Failed to create a shared memory bitmap";
      return false;
    }
  }
  unsigned char* pixels = frame_resource->shared_bitmap_->pixels();
  DCHECK(pixels);
  SkImageInfo image_info = SkImageInfo::Make(
      width_, height_, kN32_SkColorType,
      image->IsPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  // TODO(xlai): Optimize to avoid copying pixels. See crbug.com/651456.
  // However, in the case when |image| is texture backed, this function call
  // does a GPU readback which is required.
  image->ImageForCurrentFrame()->readPixels(image_info, pixels,
                                            image_info.minRowBytes(), 0, 0);
  resource.mailbox_holder.mailbox = frame_resource->shared_bitmap_->id();
  resource.mailbox_holder.texture_target = 0;
  resource.is_software = true;

  resources_.insert(next_resource_id_, std::move(frame_resource));
  return true;
}

bool OffscreenCanvasFrameDispatcherImpl::
    SetTransferableResourceToGpuMemoryBuffer(cc::TransferableResource& resource,
                                             RefPtr<StaticBitmapImage> image,
                                             const SkIRect& damage_rect,
                                             bool image_uses_gpu) {
  gpu::gles2::GLES2Interface* gl = SharedGpuContext::Gl();

  std::unique_ptr<FrameResource> frame_resource =
      createOrRecycleFrameResource();

  SkIRect update_rect;
  gfx::Size size(image->width(), image->height());
  gfx::BufferUsage buffer_usage =
      image_uses_gpu ? gfx::BufferUsage::SCANOUT
                     : gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;
  gfx::BufferFormat buffer_format =
      image->CurrentFrameKnownToBeOpaque()
          ? (SK_B32_SHIFT ? gfx::BufferFormat::RGBX_8888
                          : gfx::BufferFormat::BGRX_8888)
          : (SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                          : gfx::BufferFormat::BGRA_8888);

  if (!gpu_memory_buffer_ || gpu_memory_buffer_->GetSize() != size ||
      buffer_usage != buffer_usage_ || buffer_format != buffer_format_) {
    gpu_memory_buffer_ =
        Platform::Current()->GetGpuMemoryBufferManager()->CreateGpuMemoryBuffer(
            size, buffer_format, buffer_usage, gpu::kNullSurfaceHandle);

    if (!gpu_memory_buffer_) {
      LOG(ERROR)
          << "Failed to create GpuMemoryBuffer, maybe config is not supported";
      return false;
    }
    buffer_usage_ = buffer_usage;
    buffer_format_ = buffer_format;
    update_rect = SkIRect::MakeWH(image->width(), image->height());
  } else {
    update_rect = damage_rect;
  }

  // Software path
  // TODO(crbug.com/646022): Differentiate low latency vs. regular.  This
  // implementation is the low latency one because it may draw into the
  // GpuMemoryBuffer of the previous frame, which may result in tearing.
  if (!image_uses_gpu) {
    // Map buffer for writing.
    if (!gpu_memory_buffer_->Map()) {
      LOG(ERROR) << "Failed to map GPU memory buffer";
      return false;
    }

    int dst_stride = gpu_memory_buffer_->stride(0);
    uint8_t* gpu_memory_buffer_data =
        static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    uint8_t* dst_data = gpu_memory_buffer_data + update_rect.y() * dst_stride +
                        4 * update_rect.x();
    SkImageInfo dst_info =
        SkImageInfo::MakeN32Premul(update_rect.width(), update_rect.height());
    image->ImageForCurrentFrame()->readPixels(dst_info, dst_data, dst_stride,
                                              update_rect.x(), update_rect.y());
    // Unmap to flush writes to buffer.
    gpu_memory_buffer_->Unmap();
  }

  const GLenum format = SK_B32_SHIFT ? GL_RGBA : GL_BGRA_EXT;

  if (frame_resource->texture_id_ == 0u) {
    gl->GenTextures(1, &frame_resource->texture_id_);
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(GL_TEXTURE_2D, frame_resource->texture_id_);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->GenMailboxCHROMIUM(frame_resource->mailbox_.name);
    gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, frame_resource->mailbox_.name);
  } else {
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(GL_TEXTURE_2D, frame_resource->texture_id_);
  }

  if (frame_resource->image_id_) {
    gl->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, frame_resource->image_id_);
  } else {
    frame_resource->image_id_ =
        gl->CreateImageCHROMIUM(gpu_memory_buffer_->AsClientBuffer(),
                                image->width(), image->height(), format);
    if (!frame_resource->image_id_) {
      LOG(ERROR) << "Failed to create image";
      return false;
    }
  }
  gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, frame_resource->image_id_);

  // GPU path
  if (image_uses_gpu) {
    NOTREACHED();  // not yet implemented
    // TODO(junov): copy update_rect from canvas backing texture to
    // GpuMemoryBuffer texture.
  }

  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  resource.mailbox_holder =
      gpu::MailboxHolder(frame_resource->mailbox_, sync_token, GL_TEXTURE_2D);
  resource.is_overlay_candidate = true;
  resource.read_lock_fences_enabled = true;
  resource.is_software = false;

  SharedGpuContext::Gr()->resetContext(kTextureBinding_GrGLBackendState);

  resources_.insert(next_resource_id_, std::move(frame_resource));
  return true;
}

bool OffscreenCanvasFrameDispatcherImpl::
    SetTransferableResourceToStaticBitmapImage(
        cc::TransferableResource& resource,
        RefPtr<StaticBitmapImage> image) {
  image->EnsureMailbox();
  resource.mailbox_holder = gpu::MailboxHolder(
      image->GetMailbox(), image->GetSyncToken(), GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  // TODO(junov): crbug.com/725919 Recycle mailboxes for this code path. This is
  // hard to do because the texture associated with the mailbox gets recycled
  // through skia and skia does not store mailbox names.
  std::unique_ptr<FrameResource> frame_resource =
      createOrRecycleFrameResource();
  frame_resource->image_ = std::move(image);
  resources_.insert(next_resource_id_, std::move(frame_resource));
  return true;
}

namespace {

void UpdatePlaceholderImage(WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
                            RefPtr<WebTaskRunner> task_runner,
                            int placeholder_canvas_id,
                            RefPtr<blink::StaticBitmapImage> image,
                            unsigned resource_id) {
  DCHECK(IsMainThread());
  OffscreenCanvasPlaceholder* placeholder_canvas =
      OffscreenCanvasPlaceholder::GetPlaceholderById(placeholder_canvas_id);
  if (placeholder_canvas) {
    placeholder_canvas->SetPlaceholderFrame(
        std::move(image), std::move(dispatcher), std::move(task_runner),
        resource_id);
  }
}

}  // namespace

void OffscreenCanvasFrameDispatcherImpl::PostImageToPlaceholder(
    RefPtr<StaticBitmapImage> image) {
  // After this point, |image| can only be used on the main thread, until
  // it is returned.
  image->Transfer();
  RefPtr<WebTaskRunner> dispatcher_task_runner =
      Platform::Current()->CurrentThread()->GetWebTaskRunner();

  Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(UpdatePlaceholderImage, this->CreateWeakPtr(),
                      WTF::Passed(std::move(dispatcher_task_runner)),
                      placeholder_canvas_id_, std::move(image),
                      next_resource_id_));
}

void OffscreenCanvasFrameDispatcherImpl::DispatchFrame(
    RefPtr<StaticBitmapImage> image,
    double commit_start_time,
    const SkIRect& damage_rect,
    bool is_web_gl_software_rendering,
    GpuMemoryBufferMode mode) {
  if (!image || !VerifyImageSize(image->Size()))
    return;
  if (!frame_sink_id_.is_valid()) {
    PostImageToPlaceholder(std::move(image));
    return;
  }
  cc::CompositorFrame frame;
  // TODO(crbug.com/652931): update the device_scale_factor
  frame.metadata.device_scale_factor = 1.0f;
  if (current_begin_frame_ack_.sequence_number ==
      cc::BeginFrameArgs::kInvalidFrameNumber) {
    // TODO(eseckler): This shouldn't be necessary when OffscreenCanvas no
    // longer submits CompositorFrames without prior BeginFrame.
    current_begin_frame_ack_ = cc::BeginFrameAck::CreateManualAckWithDamage();
  } else {
    current_begin_frame_ack_.has_damage = true;
  }
  frame.metadata.begin_frame_ack = current_begin_frame_ack_;

  const gfx::Rect bounds(width_, height_);
  const int kRenderPassId = 1;
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetNew(kRenderPassId, bounds,
               gfx::Rect(damage_rect.x(), damage_rect.y(), damage_rect.width(),
                         damage_rect.height()),
               gfx::Transform());

  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds, bounds, bounds, false, 1.f,
              SkBlendMode::kSrcOver, 0);

  cc::TransferableResource resource;
  resource.id = next_resource_id_;
  resource.format = cc::ResourceFormat::RGBA_8888;
  resource.size = gfx::Size(width_, height_);
  // This indicates the filtering on the resource inherently, not the desired
  // filtering effect on the quad.
  resource.filter = GL_NEAREST;
  resource.is_overlay_candidate = false;  // may be set to true later

  bool yflipped = false;
  bool is_resource_valid = false;
  OffscreenCanvasCommitType commit_type;

  bool image_uses_gpu =
      image->IsTextureBacked() && !is_web_gl_software_rendering;

  if (mode == kUseGpuMemoryBuffer) {
    DCHECK(Platform::Current()->IsGPUCompositingEnabled());
    is_resource_valid = SetTransferableResourceToGpuMemoryBuffer(
        resource, image, damage_rect, image_uses_gpu);
    commit_type = image_uses_gpu ? kCommitGPUCanvasGPUMemoryBuffer
                                 : kCommitSoftwareCanvasGPUMemoryBuffer;
  } else {
    if (image_uses_gpu && Platform::Current()->IsGPUCompositingEnabled()) {
      commit_type = kCommitGPUCanvasGPUCompositing;
      yflipped = true;
      is_resource_valid =
          SetTransferableResourceToStaticBitmapImage(resource, image);
    } else {
      // When GPU compositing is enabled, this path could work, but it is
      // better to use GpuMemoryBuffers to pass non-gpu frames.
      DCHECK(!Platform::Current()->IsGPUCompositingEnabled());
      commit_type = image_uses_gpu ? kCommitGPUCanvasSoftwareCompositing
                                   : kCommitSoftwareCanvasSoftwareCompositing;
      is_resource_valid =
          SetTransferableResourceToSharedBitmap(resource, image);
    }
  }

  if (!is_resource_valid) {
    LOG(ERROR) << "OffscreenCanvas frame update failed because the necessary "
                  "resources could not be created";
    return;
  }

  PostImageToPlaceholder(std::move(image));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, commit_type_histogram,
      ("OffscreenCanvas.CommitType", kOffscreenCanvasCommitTypeCount));
  commit_type_histogram.Count(commit_type);

  next_resource_id_++;
  frame.resource_list.push_back(std::move(resource));

  cc::TextureDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  gfx::Size rect_size(width_, height_);

  // TODO(crbug.com/705019): optimize for contexts that have {alpha: false}
  const bool kNeedsBlending = true;
  gfx::Rect opaque_rect(0, 0);

  // TOOD(crbug.com/645993): this should be inherited from WebGL context's
  // creation settings.
  const bool kPremultipliedAlpha = true;
  const gfx::PointF uv_top_left(0.f, 0.f);
  const gfx::PointF uv_bottom_right(1.f, 1.f);
  float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
  // TODO(crbug.com/645994): this should be true when using style
  // "image-rendering: pixelated".
  // TODO(crbug.com/645590): filter should respect the image-rendering CSS
  // property of associated canvas element.
  const bool kNearestNeighbor = false;
  quad->SetAll(sqs, bounds, opaque_rect, bounds, kNeedsBlending, resource.id,
               gfx::Size(), kPremultipliedAlpha, uv_top_left, uv_bottom_right,
               SK_ColorTRANSPARENT, vertex_opacity, yflipped, kNearestNeighbor,
               false);

  frame.render_pass_list.push_back(std::move(pass));

  double elapsed_time = WTF::MonotonicallyIncreasingTime() - commit_start_time;

  switch (commit_type) {
    case kCommitGPUCanvasGPUCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, commit_gpu_canvas_gpu_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasGPUCompositingMain", 0,
             10000000, 50));
        commit_gpu_canvas_gpu_compositing_main_timer.Count(elapsed_time *
                                                           1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_gpu_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasGPUCompositingWorker", 0,
             10000000, 50));
        commit_gpu_canvas_gpu_compositing_worker_timer.Count(elapsed_time *
                                                             1000000.0);
      }
      break;
    case kCommitSoftwareCanvasGPUMemoryBuffer:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_gpu_memory_buffer_main_timer,
            ("Blink.Canvas.OffscreenCommit.SoftwareCanvasGPUMemoryBufferMain",
             0, 10000000, 50));
        commit_software_canvas_gpu_memory_buffer_main_timer.Count(elapsed_time *
                                                                  1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_gpu_memory_buffer_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasGPUMemoryBufferWorker",
             0, 10000000, 50));
        commit_software_canvas_gpu_memory_buffer_worker_timer.Count(
            elapsed_time * 1000000.0);
      }
      break;
    case kCommitSoftwareCanvasSoftwareCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_software_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasSoftwareCompositingMain",
             0, 10000000, 50));
        commit_software_canvas_software_compositing_main_timer.Count(
            elapsed_time * 1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_software_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasSoftwareCompositingWorker",
             0, 10000000, 50));
        commit_software_canvas_software_compositing_worker_timer.Count(
            elapsed_time * 1000000.0);
      }
      break;
    case kCommitGPUCanvasSoftwareCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram,
                            commit_gpu_canvas_software_compositing_main_timer,
                            ("Blink.Canvas.OffscreenCommit."
                             "GpuCanvasSoftwareCompositingMain",
                             0, 10000000, 50));
        commit_gpu_canvas_software_compositing_main_timer.Count(elapsed_time *
                                                                1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_software_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "GPUCanvasSoftwareCompositingWorker",
             0, 10000000, 50));
        commit_gpu_canvas_software_compositing_worker_timer.Count(elapsed_time *
                                                                  1000000.0);
      }
      break;
    case kCommitGPUCanvasGPUMemoryBuffer:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram,
                            commit_gpu_canvas_gpu_memory_buffer_main_timer,
                            ("Blink.Canvas.OffscreenCommit."
                             "GPUCanvasGPUMemoryBufferMain",
                             0, 10000000, 50));
        commit_gpu_canvas_gpu_memory_buffer_main_timer.Count(elapsed_time *
                                                             1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_gpu_memory_buffer_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "GPUCanvasGPUMemoryBufferWorker",
             0, 10000000, 50));
        commit_gpu_canvas_gpu_memory_buffer_worker_timer.Count(elapsed_time *
                                                               1000000.0);
      }
      break;
    default:
      NOTREACHED();
  }

  if (change_size_for_next_commit_) {
    current_local_surface_id_ = local_surface_id_allocator_.GenerateId();
    change_size_for_next_commit_ = false;
  }

  pending_compositor_frames_++;
  sink_->SubmitCompositorFrame(current_local_surface_id_, std::move(frame));
}

void OffscreenCanvasFrameDispatcherImpl::DidReceiveCompositorFrameAck(
    const cc::ReturnedResourceArray& resources) {
  ReclaimResources(resources);
  pending_compositor_frames_--;
  DCHECK_GE(pending_compositor_frames_, 0);
}

void OffscreenCanvasFrameDispatcherImpl::SetNeedsBeginFrame(
    bool needs_begin_frame) {
  if (needs_begin_frame_ == needs_begin_frame)
    return;
  needs_begin_frame_ = needs_begin_frame;
  if (!suspend_animation_)
    SetNeedsBeginFrameInternal();
}

void OffscreenCanvasFrameDispatcherImpl::SetSuspendAnimation(
    bool suspend_animation) {
  if (suspend_animation_ == suspend_animation)
    return;
  suspend_animation_ = suspend_animation;
  if (needs_begin_frame_)
    SetNeedsBeginFrameInternal();
}

void OffscreenCanvasFrameDispatcherImpl::SetNeedsBeginFrameInternal() {
  if (sink_) {
    sink_->SetNeedsBeginFrame(needs_begin_frame_ && !suspend_animation_);
  }
}

void OffscreenCanvasFrameDispatcherImpl::OnBeginFrame(
    const cc::BeginFrameArgs& begin_frame_args) {
  DCHECK(Client());

  // TODO(eseckler): Set correct |latest_confirmed_sequence_number|.
  current_begin_frame_ack_ = cc::BeginFrameAck(
      begin_frame_args.source_id, begin_frame_args.sequence_number,
      begin_frame_args.sequence_number, false);

  if (pending_compositor_frames_ >= kMaxPendingCompositorFrames ||
      (begin_frame_args.type == cc::BeginFrameArgs::MISSED &&
       base::TimeTicks::Now() > begin_frame_args.deadline)) {
    sink_->DidNotProduceFrame(current_begin_frame_ack_);
    return;
  }

  Client()->BeginFrame();
  // TODO(eseckler): Tell |m_sink| if we did not draw during the BeginFrame.
  current_begin_frame_ack_.sequence_number =
      cc::BeginFrameArgs::kInvalidFrameNumber;
}

OffscreenCanvasFrameDispatcherImpl::FrameResource::~FrameResource() {
  gpu::gles2::GLES2Interface* gl = SharedGpuContext::Gl();
  if (texture_id_)
    gl->DeleteTextures(1, &texture_id_);
  if (image_id_)
    gl->DestroyImageCHROMIUM(image_id_);
}

void OffscreenCanvasFrameDispatcherImpl::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  for (const auto& resource : resources) {
    auto it = resources_.find(resource.id);

    DCHECK(it != resources_.end());
    if (it == resources_.end())
      continue;

    if (SharedGpuContext::IsValid() && resource.sync_token.HasData()) {
      SharedGpuContext::Gl()->WaitSyncTokenCHROMIUM(
          resource.sync_token.GetConstData());
    }
    ReclaimResourceInternal(it);
  }
}

void OffscreenCanvasFrameDispatcherImpl::ReclaimResource(unsigned resource_id) {
  auto it = resources_.find(resource_id);
  if (it != resources_.end()) {
    ReclaimResourceInternal(it);
  }
}

void OffscreenCanvasFrameDispatcherImpl::ReclaimResourceInternal(
    const ResourceMap::iterator& it) {
  if (it->value->spare_lock_) {
    it->value->spare_lock_ = false;
  } else {
    // Really reclaim the resources
    recycleable_resource_ = std::move(it->value);
    // release SkImage immediately since it is not recycleable
    recycleable_resource_->image_ = nullptr;
    resources_.erase(it);
  }
}

bool OffscreenCanvasFrameDispatcherImpl::VerifyImageSize(
    const IntSize image_size) {
  if (image_size.Width() == width_ && image_size.Height() == height_)
    return true;
  return false;
}

void OffscreenCanvasFrameDispatcherImpl::Reshape(int width, int height) {
  if (width_ != width || height_ != height) {
    width_ = width;
    height_ = height;
    change_size_for_next_commit_ = true;
  }
}

}  // namespace blink
