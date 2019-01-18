// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_submitter.h"

#include <vector>

#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/filter_operations.h"
#include "cc/scheduler/video_frame_controller.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

namespace blink {

namespace {

// Delay to retry getting the context_provider.
constexpr base::TimeDelta kGetContextProviderRetryTimeout =
    base::TimeDelta::FromMilliseconds(150);

}  // namespace

VideoFrameSubmitter::VideoFrameSubmitter(
    WebContextProviderCallback context_provider_callback,
    std::unique_ptr<VideoFrameResourceProvider> resource_provider)
    : binding_(this),
      context_provider_callback_(context_provider_callback),
      resource_provider_(std::move(resource_provider)),
      rotation_(media::VIDEO_ROTATION_0),
      enable_surface_synchronization_(
          ::features::IsSurfaceSynchronizationEnabled()),
      weak_ptr_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

VideoFrameSubmitter::~VideoFrameSubmitter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (context_provider_)
    context_provider_->RemoveObserver(this);

  // Release VideoFrameResourceProvider early since its destruction will make
  // calls back into this class via the viz::SharedBitmapReporter interface.
  resource_provider_.reset();
}

void VideoFrameSubmitter::SetRotation(media::VideoRotation rotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  rotation_ = rotation;
}

void VideoFrameSubmitter::EnableSubmission(
    viz::SurfaceId surface_id,
    base::TimeTicks local_surface_id_allocation_time,
    WebFrameSinkDestroyedCallback frame_sink_destroyed_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(lethalantidote): Set these fields earlier in the constructor. Will
  // need to construct VideoFrameSubmitter later in order to do this.
  frame_sink_id_ = surface_id.frame_sink_id();
  frame_sink_destroyed_callback_ = frame_sink_destroyed_callback;
  child_local_surface_id_allocator_.UpdateFromParent(
      viz::LocalSurfaceIdAllocation(surface_id.local_surface_id(),
                                    local_surface_id_allocation_time));
  if (resource_provider_->IsInitialized())
    StartSubmitting();
}

void VideoFrameSubmitter::SetIsSurfaceVisible(bool is_visible) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_surface_visible_ = is_visible;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::SetIsPageVisible(bool is_visible) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_page_visible_ = is_visible;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::SetForceSubmit(bool force_submit) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  force_submit_ = force_submit;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::UpdateSubmissionState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!compositor_frame_sink_)
    return;

  compositor_frame_sink_->SetNeedsBeginFrame(IsDrivingFrameUpdates());

  // TODO(dalecurtis): Document how these are responsible for saving significant
  // amounts of cc memory by dealing with off-screen resources more efficiently.
  if (ShouldSubmit())
    SubmitSingleFrame();
  else if (!frame_size_.IsEmpty())
    SubmitEmptyFrame();
}

void VideoFrameSubmitter::StopUsingProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_rendering_)
    StopRendering();
  video_frame_provider_ = nullptr;
}

void VideoFrameSubmitter::StopRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_rendering_);
  DCHECK(video_frame_provider_);

  is_rendering_ = false;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::SubmitSingleFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If we haven't gotten a valid result yet from |context_provider_callback_|
  // |resource_provider_| will remain uninitialized.
  // |video_frame_provider_| may be null if StopUsingProvider has been called,
  // which could happen if the |video_frame_provider_| is destructing while we
  // are waiting for the ContextProvider.
  if (!resource_provider_->IsInitialized() || !video_frame_provider_)
    return;

  auto video_frame = video_frame_provider_->GetCurrentFrame();
  if (!video_frame)
    return;

  // TODO(dalecurtis): This probably shouldn't be posted since it runs the risk
  // of having state change out from under it. All call sites into this method
  // should be from posted tasks so it should be safe to remove the post.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&VideoFrameSubmitter::SubmitFrame),
                     weak_ptr_factory_.GetWeakPtr(),
                     viz::BeginFrameAck::CreateManualAckWithDamage(),
                     std::move(video_frame)));

  video_frame_provider_->PutCurrentFrame();
}

bool VideoFrameSubmitter::ShouldSubmit() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return (is_surface_visible_ && is_page_visible_) || force_submit_;
}

bool VideoFrameSubmitter::IsDrivingFrameUpdates() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We drive frame updates only when we believe that something is consuming
  // them.  This is different than VideoLayer, which drives updates any time
  // they're in the layer tree.
  return is_rendering_ && ShouldSubmit();
}

void VideoFrameSubmitter::DidReceiveFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(video_frame_provider_);

  // DidReceiveFrame is called before rendering has started, as a part of
  // VideoRendererSink::PaintSingleFrame.
  if (!is_rendering_)
    SubmitSingleFrame();
}

void VideoFrameSubmitter::StartRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_rendering_);
  is_rendering_ = true;

  if (compositor_frame_sink_)
    compositor_frame_sink_->SetNeedsBeginFrame(is_rendering_ && ShouldSubmit());
}

void VideoFrameSubmitter::Initialize(cc::VideoFrameProvider* provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!provider)
    return;

  DCHECK(!video_frame_provider_);
  video_frame_provider_ = provider;
  context_provider_callback_.Run(
      nullptr, base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                              weak_ptr_factory_.GetWeakPtr()));
}

void VideoFrameSubmitter::OnReceivedContextProvider(
    bool use_gpu_compositing,
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!use_gpu_compositing) {
    resource_provider_->Initialize(nullptr, this);
    return;
  }

  bool has_good_context = false;
  while (!has_good_context) {
    if (!context_provider) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              context_provider_callback_, context_provider_,
              base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                             weak_ptr_factory_.GetWeakPtr())),
          kGetContextProviderRetryTimeout);
      return;
    }

    // Note that |context_provider| is now null after the move, such that if we
    // end up having !|has_good_context|, we will retry to obtain the
    // context_provider.
    context_provider_ = std::move(context_provider);
    auto result = context_provider_->BindToCurrentThread();

    has_good_context =
        result == gpu::ContextResult::kSuccess &&
        context_provider_->ContextGL()->GetGraphicsResetStatusKHR() ==
            GL_NO_ERROR;
  }
  context_provider_->AddObserver(this);
  resource_provider_->Initialize(context_provider_.get(), nullptr);

  if (frame_sink_id_.is_valid())
    StartSubmitting();
}

void VideoFrameSubmitter::StartSubmitting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(frame_sink_id_.is_valid());

  mojom::blink::EmbeddedFrameSinkProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));

  viz::mojom::blink::CompositorFrameSinkClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  provider->CreateCompositorFrameSink(
      frame_sink_id_, std::move(client),
      mojo::MakeRequest(&compositor_frame_sink_));
  if (!surface_embedder_.is_bound()) {
    provider->ConnectToEmbedder(frame_sink_id_,
                                mojo::MakeRequest(&surface_embedder_));
  }

  compositor_frame_sink_.set_connection_error_handler(base::BindOnce(
      &VideoFrameSubmitter::OnContextLost, base::Unretained(this)));

  UpdateSubmissionState();
}

bool VideoFrameSubmitter::SubmitFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(video_frame);
  TRACE_EVENT0("media", "VideoFrameSubmitter::SubmitFrame");

  if (!compositor_frame_sink_ || !ShouldSubmit())
    return false;

  gfx::Size frame_size(video_frame->natural_size());
  if (rotation_ == media::VIDEO_ROTATION_90 ||
      rotation_ == media::VIDEO_ROTATION_270) {
    frame_size = gfx::Size(frame_size.height(), frame_size.width());
  }
  if (frame_size_ != frame_size) {
    if (!frame_size_.IsEmpty()) {
      child_local_surface_id_allocator_.GenerateId();
      if (enable_surface_synchronization_) {
        surface_embedder_->SetLocalSurfaceId(
            child_local_surface_id_allocator_
                .GetCurrentLocalSurfaceIdAllocation()
                .local_surface_id());
      }
    }
    frame_size_ = frame_size;
  }

  viz::CompositorFrame compositor_frame;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  render_pass->SetNew(1, gfx::Rect(frame_size_), gfx::Rect(frame_size_),
                      gfx::Transform());
  render_pass->filters = cc::FilterOperations();
  resource_provider_->AppendQuads(render_pass.get(), video_frame, rotation_,
                                  media::IsOpaque(video_frame->format()));
  compositor_frame.metadata.begin_frame_ack = begin_frame_ack;
  compositor_frame.metadata.frame_token = ++next_frame_token_;

  // We don't assume that the ack is marked as having damage.  However, we're
  // definitely emitting a CompositorFrame that damages the entire surface.
  compositor_frame.metadata.begin_frame_ack.has_damage = true;
  compositor_frame.metadata.device_scale_factor = 1;
  compositor_frame.metadata.may_contain_video = true;

  std::vector<viz::ResourceId> resources;
  if (!render_pass->quad_list.empty()) {
    DCHECK_EQ(render_pass->quad_list.size(), 1u);
    resources.assign(render_pass->quad_list.front()->resources.begin(),
                     render_pass->quad_list.front()->resources.end());
  }
  resource_provider_->PrepareSendToParent(resources,
                                          &compositor_frame.resource_list);
  compositor_frame.render_pass_list.push_back(std::move(render_pass));

  const auto& current_surface =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  compositor_frame.metadata.local_surface_id_allocation_time =
      current_surface.allocation_time();

  // TODO(lethalantidote): Address third/fourth arg in SubmitCompositorFrame.
  compositor_frame_sink_->SubmitCompositorFrame(
      current_surface.local_surface_id(), std::move(compositor_frame), nullptr,
      0);
  resource_provider_->ReleaseFrameResources();

  waiting_for_compositor_ack_ = true;
  return true;
}

void VideoFrameSubmitter::SubmitEmptyFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_ && !ShouldSubmit());
  DCHECK(!frame_size_.IsEmpty());
  TRACE_EVENT0("media", "VideoFrameSubmitter::SubmitEmptyFrame");

  viz::CompositorFrame compositor_frame;

  compositor_frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  compositor_frame.metadata.frame_token = ++next_frame_token_;
  compositor_frame.metadata.device_scale_factor = 1;
  compositor_frame.metadata.may_contain_video = true;

  const auto& current_surface =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();

  compositor_frame.metadata.local_surface_id_allocation_time =
      current_surface.allocation_time();

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(frame_size_), gfx::Rect(frame_size_),
                      gfx::Transform());
  compositor_frame.render_pass_list.push_back(std::move(render_pass));

  compositor_frame_sink_->SubmitCompositorFrame(
      current_surface.local_surface_id(), std::move(compositor_frame), nullptr,
      0);
  waiting_for_compositor_ack_ = true;
}

void VideoFrameSubmitter::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    WTF::HashMap<uint32_t, ::gfx::mojom::blink::PresentationFeedbackPtr>) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("media", "VideoFrameSubmitter::OnBeginFrame");

  viz::BeginFrameAck current_begin_frame_ack(args, false);
  if (args.type == viz::BeginFrameArgs::MISSED) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  // Update the current frame, even if we haven't gotten an ack for a previous
  // frame yet.  That probably signals a dropped frame, and this will let the
  // provider know that it happened, since we won't PutCurrentFrame this one.
  // Note that we should DidNotProduceFrame with or without the ack.
  if (!video_frame_provider_ || !video_frame_provider_->UpdateCurrentFrame(
                                    args.frame_time + args.interval,
                                    args.frame_time + 2 * args.interval)) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  scoped_refptr<media::VideoFrame> video_frame =
      video_frame_provider_->GetCurrentFrame();

  // We do have a new frame that we could display.  See if we're supposed to
  // actually submit a frame or not, and try to submit one.
  if (!is_rendering_ || waiting_for_compositor_ack_ ||
      !SubmitFrame(current_begin_frame_ack, std::move(video_frame))) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  // We submitted a frame!

  // We still signal PutCurrentFrame here, rather than on the ack, so that it
  // lines up with the correct frame.  Otherwise, any intervening calls to
  // OnBeginFrame => UpdateCurrentFrame will cause the put to signal that the
  // later frame was displayed.
  video_frame_provider_->PutCurrentFrame();
}

void VideoFrameSubmitter::OnContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(lethalantidote): This check will be obsolete once other TODO to move
  // field initialization earlier is fulfilled.
  if (frame_sink_destroyed_callback_)
    frame_sink_destroyed_callback_.Run();

  if (binding_.is_bound())
    binding_.Unbind();

  if (context_provider_)
    context_provider_->RemoveObserver(this);

  waiting_for_compositor_ack_ = false;

  resource_provider_->OnContextLost();

  // |compositor_frame_sink_| should be reset last.
  compositor_frame_sink_.reset();

  context_provider_callback_.Run(
      context_provider_,
      base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                     weak_ptr_factory_.GetWeakPtr()));

  // We need to trigger another submit so that surface_id's get propagated
  // correctly. If we don't, we don't get any more signals to update the
  // submission state.
  //
  // TODO(dalecurtis, liberato): This isn't sufficient to get video working
  // again, instead when the new context comes in the old frame should be
  // submitted or if rendering is in progress, nothing should be done.
  is_surface_visible_ = true;
}

void VideoFrameSubmitter::DidReceiveCompositorFrameAck(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReclaimResources(resources);
  waiting_for_compositor_ack_ = false;
}

void VideoFrameSubmitter::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebVector<viz::ReturnedResource> temp_resources = resources;
  std::vector<viz::ReturnedResource> std_resources =
      temp_resources.ReleaseVector();
  resource_provider_->ReceiveReturnsFromParent(std_resources);
}

void VideoFrameSubmitter::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const viz::SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_);
  compositor_frame_sink_->DidAllocateSharedBitmap(
      std::move(buffer), SharedBitmapIdToGpuMailboxPtr(id));
}

void VideoFrameSubmitter::DidDeleteSharedBitmap(const viz::SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_);
  compositor_frame_sink_->DidDeleteSharedBitmap(
      SharedBitmapIdToGpuMailboxPtr(id));
}

void VideoFrameSubmitter::SetSurfaceIdForTesting(
    const viz::SurfaceId& surface_id,
    base::TimeTicks allocation_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  frame_sink_id_ = surface_id.frame_sink_id();
  child_local_surface_id_allocator_.UpdateFromParent(
      viz::LocalSurfaceIdAllocation(surface_id.local_surface_id(),
                                    allocation_time));
}

}  // namespace blink
