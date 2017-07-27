// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"

#include "cc/output/compositor_frame.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/viz/common/quads/resource_format.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/OffscreenCanvasPlaceholder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"
#include "platform/wtf/typed_arrays/Uint8Array.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/rect.h"
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
      frame_sink_id_(viz::FrameSinkId(client_id, sink_id)),
      width_(width),
      height_(height),
      change_size_for_next_commit_(false),
      needs_begin_frame_(false),
      next_resource_id_(1u),
      binding_(this),
      placeholder_canvas_id_(canvas_id) {
  if (frame_sink_id_.is_valid()) {
    // Only frameless canvas pass an invalid frame sink id; we don't create
    // mojo channel for this special case.
    current_local_surface_id_ = local_surface_id_allocator_.GenerateId();
    DCHECK(!sink_.is_bound());
    mojom::blink::OffscreenCanvasProviderPtr provider;
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&provider));

    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    auto scheduler = blink::Platform::Current()->CurrentThread()->Scheduler();
    if (scheduler) {
      WebTaskRunner* web_task_runner = scheduler->CompositorTaskRunner();
      if (web_task_runner) {
        task_runner = web_task_runner->ToSingleThreadTaskRunner();
      }
    }
    viz::mojom::blink::CompositorFrameSinkClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client), task_runner);
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

void OffscreenCanvasFrameDispatcherImpl::SetTransferableResourceToSharedBitmap(
    cc::TransferableResource& resource,
    RefPtr<StaticBitmapImage> image) {
  std::unique_ptr<FrameResource> frame_resource =
      createOrRecycleFrameResource();
  if (!frame_resource->shared_bitmap_) {
    frame_resource->shared_bitmap_ =
        Platform::Current()->AllocateSharedBitmap(IntSize(width_, height_));
    if (!frame_resource->shared_bitmap_)
      return;
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
}

void OffscreenCanvasFrameDispatcherImpl::
    SetTransferableResourceToSharedGPUContext(
        cc::TransferableResource& resource,
        RefPtr<StaticBitmapImage> image) {
  DCHECK(!image->IsTextureBacked());

  // TODO(crbug.com/652707): When committing the first frame, there is no
  // instance of SharedGpuContext yet, calling SharedGpuContext::gl() will
  // trigger a creation of an instace, which requires to create a
  // WebGraphicsContext3DProvider. This process is quite expensive, because
  // WebGraphicsContext3DProvider can only be constructed on the main thread,
  // and bind to the worker thread if commit() is called on worker. In the
  // subsequent frame, we should already have a SharedGpuContext, then getting
  // the gl interface should not be expensive.
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  gpu::gles2::GLES2Interface* gl =
      context_provider_wrapper->ContextProvider()->ContextGL();
  GrContext* gr = context_provider_wrapper->ContextProvider()->GetGrContext();
  if (!gl || !gr)
    return;

  std::unique_ptr<FrameResource> frame_resource =
      createOrRecycleFrameResource();

  SkImageInfo info = SkImageInfo::Make(
      width_, height_, kN32_SkColorType,
      image->IsPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  RefPtr<ArrayBuffer> dst_buffer =
      ArrayBuffer::CreateOrNull(width_ * height_, info.bytesPerPixel());
  // If it fails to create a buffer for copying the pixel data, then exit early.
  if (!dst_buffer)
    return;
  unsigned byte_length = dst_buffer->ByteLength();
  RefPtr<Uint8Array> dst_pixels =
      Uint8Array::Create(std::move(dst_buffer), 0, byte_length);
  image->ImageForCurrentFrame()->readPixels(info, dst_pixels->Data(),
                                            info.minRowBytes(), 0, 0);
  DCHECK(frame_resource->context_provider_wrapper_.get() ==
             context_provider_wrapper.get() ||
         !frame_resource->context_provider_wrapper_);

  if (frame_resource->texture_id_ == 0u ||
      !frame_resource->context_provider_wrapper_) {
    frame_resource->context_provider_wrapper_ = context_provider_wrapper;
    gl->GenTextures(1, &frame_resource->texture_id_);
    gl->BindTexture(GL_TEXTURE_2D, frame_resource->texture_id_);
    GLenum format =
        (kN32_SkColorType == kRGBA_8888_SkColorType) ? GL_RGBA : GL_BGRA_EXT;
    gl->TexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format,
                   GL_UNSIGNED_BYTE, 0);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, format,
                      GL_UNSIGNED_BYTE, dst_pixels->Data());

    gl->GenMailboxCHROMIUM(frame_resource->mailbox_.name);
    gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, frame_resource->mailbox_.name);
  }

  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  resource.mailbox_holder =
      gpu::MailboxHolder(frame_resource->mailbox_, sync_token, GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  resources_.insert(next_resource_id_, std::move(frame_resource));
  gr->resetContext(kTextureBinding_GrGLBackendState);
}

void OffscreenCanvasFrameDispatcherImpl::
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

  Platform::Current()
      ->MainThread()
      ->Scheduler()
      ->CompositorTaskRunner()
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(UpdatePlaceholderImage, this->CreateWeakPtr(),
                                 WTF::Passed(std::move(dispatcher_task_runner)),
                                 placeholder_canvas_id_, std::move(image),
                                 next_resource_id_));
}

void OffscreenCanvasFrameDispatcherImpl::DispatchFrame(
    RefPtr<StaticBitmapImage> image,
    double commit_start_time,
    const SkIRect& damage_rect,
    bool is_web_gl_software_rendering /* This flag is true when WebGL's commit
                                         is called on SwiftShader. */
    ) {
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
      viz::BeginFrameArgs::kInvalidFrameNumber) {
    // TODO(eseckler): This shouldn't be necessary when OffscreenCanvas no
    // longer submits CompositorFrames without prior BeginFrame.
    current_begin_frame_ack_ = viz::BeginFrameAck::CreateManualAckWithDamage();
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
  resource.format = viz::ResourceFormat::RGBA_8888;
  resource.size = gfx::Size(width_, height_);
  // This indicates the filtering on the resource inherently, not the desired
  // filtering effect on the quad.
  resource.filter = GL_NEAREST;
  // TODO(crbug.com/646022): making this overlay-able.
  resource.is_overlay_candidate = false;

  bool yflipped = false;
  OffscreenCanvasCommitType commit_type;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, commit_type_histogram,
      ("OffscreenCanvas.CommitType", kOffscreenCanvasCommitTypeCount));
  if (image->IsTextureBacked()) {
    if (Platform::Current()->IsGPUCompositingEnabled() &&
        !is_web_gl_software_rendering) {
      // Case 1: both canvas and compositor are gpu accelerated.
      commit_type = kCommitGPUCanvasGPUCompositing;
      SetTransferableResourceToStaticBitmapImage(resource, image);
      yflipped = true;
    } else {
      // Case 2: canvas is accelerated but --disable-gpu-compositing is
      // specified, or WebGL's commit is called with SwiftShader. The latter
      // case is indicated by
      // WebGraphicsContext3DProvider::isSoftwareRendering.
      commit_type = kCommitGPUCanvasSoftwareCompositing;
      SetTransferableResourceToSharedBitmap(resource, image);
    }
  } else {
    if (Platform::Current()->IsGPUCompositingEnabled() &&
        !is_web_gl_software_rendering) {
      // Case 3: canvas is not gpu-accelerated, but compositor is
      commit_type = kCommitSoftwareCanvasGPUCompositing;
      SetTransferableResourceToSharedGPUContext(resource, image);
    } else {
      // Case 4: both canvas and compositor are not gpu accelerated.
      commit_type = kCommitSoftwareCanvasSoftwareCompositing;
      SetTransferableResourceToSharedBitmap(resource, image);
    }
  }

  PostImageToPlaceholder(std::move(image));
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
    case kCommitGPUCanvasSoftwareCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_software_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasSoftwareCompositingMain", 0,
             10000000, 50));
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
    case kCommitSoftwareCanvasGPUCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_gpu_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit.SoftwareCanvasGPUCompositingMain", 0,
             10000000, 50));
        commit_software_canvas_gpu_compositing_main_timer.Count(elapsed_time *
                                                                1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_gpu_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasGPUCompositingWorker",
             0, 10000000, 50));
        commit_software_canvas_gpu_compositing_worker_timer.Count(elapsed_time *
                                                                  1000000.0);
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
    case kOffscreenCanvasCommitTypeCount:
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
    const WTF::Vector<cc::ReturnedResource>& resources) {
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
    const viz::BeginFrameArgs& begin_frame_args) {
  DCHECK(Client());

  current_begin_frame_ack_ = viz::BeginFrameAck(
      begin_frame_args.source_id, begin_frame_args.sequence_number, false);

  if (pending_compositor_frames_ >= kMaxPendingCompositorFrames ||
      (begin_frame_args.type == viz::BeginFrameArgs::MISSED &&
       base::TimeTicks::Now() > begin_frame_args.deadline)) {
    sink_->DidNotProduceFrame(current_begin_frame_ack_);
    return;
  }

  Client()->BeginFrame();
  // TODO(eseckler): Tell |m_sink| if we did not draw during the BeginFrame.
  current_begin_frame_ack_.sequence_number =
      viz::BeginFrameArgs::kInvalidFrameNumber;
}

OffscreenCanvasFrameDispatcherImpl::FrameResource::~FrameResource() {
  if (context_provider_wrapper_) {
    gpu::gles2::GLES2Interface* gl =
        context_provider_wrapper_->ContextProvider()->ContextGL();
    if (texture_id_)
      gl->DeleteTextures(1, &texture_id_);
    if (image_id_)
      gl->DestroyImageCHROMIUM(image_id_);
  }
}

void OffscreenCanvasFrameDispatcherImpl::ReclaimResources(
    const WTF::Vector<cc::ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    auto it = resources_.find(resource.id);

    DCHECK(it != resources_.end());
    if (it == resources_.end())
      continue;

    if (it->value->context_provider_wrapper_ && resource.sync_token.HasData()) {
      it->value->context_provider_wrapper_->ContextProvider()
          ->ContextGL()
          ->WaitSyncTokenCHROMIUM(resource.sync_token.GetConstData());
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
