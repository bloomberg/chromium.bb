// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/VideoFrameSubmitter.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/filter_operations.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/video_resource_updater.h"
#include "cc/scheduler/video_frame_controller.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "media/base/video_frame.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"

namespace blink {

VideoFrameSubmitter::VideoFrameSubmitter(
    std::unique_ptr<VideoFrameResourceProvider> resource_provider)
    : binding_(this),
      resource_provider_(std::move(resource_provider)),
      is_rendering_(false),
      weak_ptr_factory_(this) {
  current_local_surface_id_ = parent_local_surface_id_allocator_.GenerateId();
  DETACH_FROM_THREAD(media_thread_checker_);
}

VideoFrameSubmitter::~VideoFrameSubmitter() = default;

void VideoFrameSubmitter::StopUsingProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (is_rendering_)
    StopRendering();
  provider_ = nullptr;
}

void VideoFrameSubmitter::StopRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(is_rendering_);
  DCHECK(provider_);

  // Push out final frame.
  SubmitSingleFrame();

  is_rendering_ = false;
  compositor_frame_sink_->SetNeedsBeginFrame(false);
}

void VideoFrameSubmitter::SubmitSingleFrame() {
  viz::BeginFrameAck current_begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  scoped_refptr<media::VideoFrame> video_frame = provider_->GetCurrentFrame();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameSubmitter::SubmitFrame,
                                weak_ptr_factory_.GetWeakPtr(),
                                current_begin_frame_ack, video_frame));
  provider_->PutCurrentFrame();
}

void VideoFrameSubmitter::DidReceiveFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(provider_);

  // DidReceiveFrame is called before renderering has started, as a part of
  // PaintSingleFrame.
  if (!is_rendering_) {
    SubmitSingleFrame();
  }
}

void VideoFrameSubmitter::StartRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(!is_rendering_);
  compositor_frame_sink_->SetNeedsBeginFrame(true);
  is_rendering_ = true;
}

void VideoFrameSubmitter::Initialize(cc::VideoFrameProvider* provider) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (provider) {
    DCHECK(!provider_);
    provider_ = provider;
    resource_provider_->ObtainContextProvider();
  }
}

void VideoFrameSubmitter::StartSubmitting(const viz::FrameSinkId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(id.is_valid());

  // TODO(lethalantidote): Class to be renamed.
  mojom::blink::OffscreenCanvasProviderPtr canvas_provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&canvas_provider));

  viz::mojom::blink::CompositorFrameSinkClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  canvas_provider->CreateCompositorFrameSink(
      id, std::move(client), mojo::MakeRequest(&compositor_frame_sink_));

  scoped_refptr<media::VideoFrame> video_frame = provider_->GetCurrentFrame();
  if (video_frame) {
    viz::BeginFrameAck current_begin_frame_ack =
        viz::BeginFrameAck::CreateManualAckWithDamage();
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&VideoFrameSubmitter::SubmitFrame,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  current_begin_frame_ack, video_frame));
    provider_->PutCurrentFrame();
  }
}

void VideoFrameSubmitter::SubmitFrame(
    viz::BeginFrameAck begin_frame_ack,
    scoped_refptr<media::VideoFrame> video_frame) {
  TRACE_EVENT0("media", "VideoFrameSubmitter::SubmitFrame");
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(compositor_frame_sink_);

  viz::CompositorFrame compositor_frame;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  render_pass->SetNew(1, gfx::Rect(video_frame->coded_size()),
                      gfx::Rect(video_frame->coded_size()), gfx::Transform());
  render_pass->filters = cc::FilterOperations();
  resource_provider_->AppendQuads(render_pass.get(), video_frame, rotation_);
  compositor_frame.metadata.begin_frame_ack = begin_frame_ack;
  compositor_frame.metadata.device_scale_factor = 1;
  compositor_frame.metadata.may_contain_video = true;

  cc::ResourceProvider::ResourceIdArray resources;
  for (auto* quad : render_pass->quad_list) {
    for (viz::ResourceId resource_id : quad->resources) {
      resources.push_back(resource_id);
    }
  }
  resource_provider_->PrepareSendToParent(resources,
                                          &compositor_frame.resource_list);
  compositor_frame.render_pass_list.push_back(std::move(render_pass));

  if (compositor_frame.size_in_pixels() != current_size_in_pixels_) {
    current_local_surface_id_ = parent_local_surface_id_allocator_.GenerateId();
    current_size_in_pixels_ = compositor_frame.size_in_pixels();
  }

  // TODO(lethalantidote): Address third/fourth arg in SubmitCompositorFrame.
  compositor_frame_sink_->SubmitCompositorFrame(
      current_local_surface_id_, std::move(compositor_frame), nullptr, 0);
  resource_provider_->ReleaseFrameResources();
}

void VideoFrameSubmitter::OnBeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT0("media", "VideoFrameSubmitter::OnBeginFrame");
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  viz::BeginFrameAck current_begin_frame_ack =
      viz::BeginFrameAck(args.source_id, args.sequence_number, false);
  if (args.type == viz::BeginFrameArgs::MISSED) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  current_begin_frame_ack.has_damage = true;

  if (!provider_ ||
      !provider_->UpdateCurrentFrame(args.frame_time + args.interval,
                                     args.frame_time + 2 * args.interval) ||
      !is_rendering_) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  scoped_refptr<media::VideoFrame> video_frame = provider_->GetCurrentFrame();

  SubmitFrame(current_begin_frame_ack, video_frame);

  provider_->PutCurrentFrame();
}

void VideoFrameSubmitter::SetRotation(media::VideoRotation rotation) {
  rotation_ = rotation;
}

void VideoFrameSubmitter::DidReceiveCompositorFrameAck(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  ReclaimResources(resources);
}

void VideoFrameSubmitter::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  WebVector<viz::ReturnedResource> temp_resources = resources;
  std::vector<viz::ReturnedResource> std_resources =
      temp_resources.ReleaseVector();
  resource_provider_->ReceiveReturnsFromParent(std_resources);
}

void VideoFrameSubmitter::DidPresentCompositorFrame(
    uint32_t presentation_token,
    mojo_base::mojom::blink::TimeTicksPtr time,
    WTF::TimeDelta refresh,
    uint32_t flags) {}

void VideoFrameSubmitter::DidDiscardCompositorFrame(
    uint32_t presentation_token) {}
}  // namespace blink
