// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"

#include <memory>
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_format.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/OffscreenCanvasPlaceholder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"

namespace blink {

enum {
  kMaxPendingCompositorFrames = 2,
  kMaxUnreclaimedPlaceholderFrames = 3,
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
      binding_(this),
      placeholder_canvas_id_(canvas_id),
      num_unreclaimed_frames_posted_(0) {
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
    if (scheduler)
      task_runner = scheduler->CompositorTaskRunner();
    viz::mojom::blink::CompositorFrameSinkClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client), task_runner);
    provider->CreateCompositorFrameSink(frame_sink_id_, std::move(client),
                                        mojo::MakeRequest(&sink_));
  }
  offscreen_canvas_resource_provider_ =
      std::make_unique<OffscreenCanvasResourceProvider>(width, height);
}

OffscreenCanvasFrameDispatcherImpl::~OffscreenCanvasFrameDispatcherImpl() {
}

namespace {

void UpdatePlaceholderImage(WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
                            scoped_refptr<WebTaskRunner> task_runner,
                            int placeholder_canvas_id,
                            scoped_refptr<blink::StaticBitmapImage> image,
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

void OffscreenCanvasFrameDispatcherImpl::PostImageToPlaceholderIfNotBlocked(
    scoped_refptr<StaticBitmapImage> image,
    unsigned resource_id) {
  if (placeholder_canvas_id_ == kInvalidPlaceholderCanvasId) {
    offscreen_canvas_resource_provider_->ReclaimResource(resource_id);
    return;
  }
  // Determines whether the main thread may be blocked. If unblocked, post the
  // image. Otherwise, save the image and do not post it.
  if (num_unreclaimed_frames_posted_ < kMaxUnreclaimedPlaceholderFrames) {
    // After this point, |image| can only be used on the main thread, until it
    // is returned.
    image->Transfer();
    this->PostImageToPlaceholder(std::move(image), resource_id);
    num_unreclaimed_frames_posted_++;
  } else {
    DCHECK(num_unreclaimed_frames_posted_ == kMaxUnreclaimedPlaceholderFrames);
    if (latest_unposted_image_) {
      // The previous unposted image becomes obsolete now.
      offscreen_canvas_resource_provider_->ReclaimResource(
          latest_unposted_resource_id_);
    }

    latest_unposted_image_ = std::move(image);
    latest_unposted_resource_id_ = resource_id;
  }
}

void OffscreenCanvasFrameDispatcherImpl::PostImageToPlaceholder(
    scoped_refptr<StaticBitmapImage> image,
    unsigned resource_id) {
  scoped_refptr<WebTaskRunner> dispatcher_task_runner =
      Platform::Current()->CurrentThread()->GetWebTaskRunner();

  Platform::Current()
      ->MainThread()
      ->Scheduler()
      ->CompositorTaskRunner()
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(UpdatePlaceholderImage, this->CreateWeakPtr(),
                                 WTF::Passed(std::move(dispatcher_task_runner)),
                                 placeholder_canvas_id_, std::move(image),
                                 resource_id));
}

void OffscreenCanvasFrameDispatcherImpl::DispatchFrame(
    scoped_refptr<StaticBitmapImage> image,
    double commit_start_time,
    const SkIRect& damage_rect) {
  if (!image || !VerifyImageSize(image->Size()))
    return;

  offscreen_canvas_resource_provider_->IncNextResourceId();

  if (!frame_sink_id_.is_valid()) {
    PostImageToPlaceholderIfNotBlocked(
        std::move(image),
        offscreen_canvas_resource_provider_->GetNextResourceId());
    return;
  }
  viz::CompositorFrame frame;
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
  bool is_clipped = false;
  // TODO(crbug.com/705019): optimize for contexts that have {alpha: false}
  bool are_contents_opaque = false;
  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  pass->SetNew(kRenderPassId, bounds,
               gfx::Rect(damage_rect.x(), damage_rect.y(), damage_rect.width(),
                         damage_rect.height()),
               gfx::Transform());

  viz::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds, bounds, bounds, is_clipped,
              are_contents_opaque, 1.f, SkBlendMode::kSrcOver, 0);

  viz::TransferableResource resource;
  offscreen_canvas_resource_provider_->TransferResource(&resource);

  bool yflipped = false;
  OffscreenCanvasCommitType commit_type;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, commit_type_histogram,
      ("OffscreenCanvas.CommitType", kOffscreenCanvasCommitTypeCount));
  if (image->IsTextureBacked()) {
    // While |image| is texture backed, it could be generated with "software
    // rendering" aka swiftshader. If the compositor is not also using
    // swiftshader, then we could not give a swiftshader based texture
    // to the compositor. However in that case, IsGpuCompositingEnabled() will
    // also be false, so we will avoid doing so.
    if (SharedGpuContext::IsGpuCompositingEnabled()) {
      // Case 1: both canvas and compositor are gpu accelerated.
      commit_type = kCommitGPUCanvasGPUCompositing;
      offscreen_canvas_resource_provider_
          ->SetTransferableResourceToStaticBitmapImage(resource, image);
      yflipped = true;
    } else {
      // Case 2: canvas is accelerated but gpu compositing is disabled.
      commit_type = kCommitGPUCanvasSoftwareCompositing;
      offscreen_canvas_resource_provider_
          ->SetTransferableResourceToSharedBitmap(resource, image);
    }
  } else {
    if (SharedGpuContext::IsGpuCompositingEnabled()) {
      // Case 3: canvas is not gpu-accelerated, but compositor is.
      commit_type = kCommitSoftwareCanvasGPUCompositing;
      offscreen_canvas_resource_provider_
          ->SetTransferableResourceToStaticBitmapImage(
              resource, image->MakeAccelerated(
                            SharedGpuContext::ContextProviderWrapper()));
    } else {
      // Case 4: both canvas and compositor are not gpu accelerated.
      commit_type = kCommitSoftwareCanvasSoftwareCompositing;
      offscreen_canvas_resource_provider_
          ->SetTransferableResourceToSharedBitmap(resource, image);
    }
  }

  commit_type_histogram.Count(commit_type);

  PostImageToPlaceholderIfNotBlocked(
      std::move(image),
      offscreen_canvas_resource_provider_->GetNextResourceId());

  frame.resource_list.push_back(std::move(resource));

  viz::TextureDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  gfx::Size rect_size(width_, height_);

  // TODO(crbug.com/705019): optimize for contexts that have {alpha: false}
  const bool kNeedsBlending = true;

  // TODO(crbug.com/645993): this should be inherited from WebGL context's
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
  quad->SetAll(sqs, bounds, bounds, kNeedsBlending, resource.id, gfx::Size(),
               kPremultipliedAlpha, uv_top_left, uv_bottom_right,
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
  sink_->SubmitCompositorFrame(current_local_surface_id_, std::move(frame),
                               nullptr, 0);
}

void OffscreenCanvasFrameDispatcherImpl::DidReceiveCompositorFrameAck(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  ReclaimResources(resources);
  pending_compositor_frames_--;
  DCHECK_GE(pending_compositor_frames_, 0);
}

void OffscreenCanvasFrameDispatcherImpl::DidPresentCompositorFrame(
    uint32_t presentation_token,
    ::mojo::common::mojom::blink::TimeTicksPtr time,
    WTF::TimeDelta refresh,
    uint32_t flags) {
  NOTIMPLEMENTED();
}

void OffscreenCanvasFrameDispatcherImpl::DidDiscardCompositorFrame(
    uint32_t presentation_token) {
  NOTIMPLEMENTED();
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
void OffscreenCanvasFrameDispatcherImpl::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  offscreen_canvas_resource_provider_->ReclaimResources(resources);
}

void OffscreenCanvasFrameDispatcherImpl::ReclaimResource(unsigned resource_id) {
  offscreen_canvas_resource_provider_->ReclaimResource(resource_id);
  num_unreclaimed_frames_posted_--;

  // The main thread has become unblocked recently and we have an image that
  // have not been posted yet.
  if (latest_unposted_image_) {
    DCHECK(num_unreclaimed_frames_posted_ ==
           kMaxUnreclaimedPlaceholderFrames - 1);
    PostImageToPlaceholderIfNotBlocked(std::move(latest_unposted_image_),
                                       latest_unposted_resource_id_);
    latest_unposted_resource_id_ = 0;
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
    offscreen_canvas_resource_provider_->Reshape(width, height);
    change_size_for_next_commit_ = true;
  }
}

}  // namespace blink
