// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display_client_compositor_frame_sink.h"

#include "base/threading/thread_checker.h"
#include "cc/output/compositor_frame_sink_client.h"

namespace ui {
namespace ws {

DisplayClientCompositorFrameSink::DisplayClientCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkAssociatedPtr compositor_frame_sink,
    cc::mojom::DisplayPrivateAssociatedPtr display_private,
    cc::mojom::MojoCompositorFrameSinkClientRequest client_request)
    : cc::CompositorFrameSink(nullptr, nullptr, nullptr, nullptr),
      compositor_frame_sink_(std::move(compositor_frame_sink)),
      client_binding_(this, std::move(client_request)),
      display_private_(std::move(display_private)),
      frame_sink_id_(frame_sink_id) {}

DisplayClientCompositorFrameSink::~DisplayClientCompositorFrameSink() {}

bool DisplayClientCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  if (!cc::CompositorFrameSink::BindToClient(client))
    return false;
  DCHECK(!thread_checker_);
  thread_checker_ = base::MakeUnique<base::ThreadChecker>();

  begin_frame_source_ = base::MakeUnique<cc::ExternalBeginFrameSource>(this);

  client->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void DisplayClientCompositorFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  cc::CompositorFrameSink::DetachFromClient();
}

void DisplayClientCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!compositor_frame_sink_)
    return;

  DCHECK_LE(cc::BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  gfx::Size frame_size = last_submitted_frame_size_;
  if (!frame.render_pass_list.empty())
    frame_size = frame.render_pass_list.back()->output_rect.size();

  if (!local_surface_id_.is_valid() ||
      frame_size != last_submitted_frame_size_) {
    local_surface_id_ = id_allocator_.GenerateId();
    display_private_->ResizeDisplay(frame_size);
  }
  display_private_->SetLocalSurfaceId(local_surface_id_,
                                      frame.metadata.device_scale_factor);
  compositor_frame_sink_->SubmitCompositorFrame(local_surface_id_,
                                                std::move(frame));
  last_submitted_frame_size_ = frame_size;
}

void DisplayClientCompositorFrameSink::DidReceiveCompositorFrameAck() {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
}

void DisplayClientCompositorFrameSink::OnBeginFrame(
    const cc::BeginFrameArgs& begin_frame_args) {
  DCHECK(thread_checker_->CalledOnValidThread());
  begin_frame_source_->OnBeginFrame(begin_frame_args);
}

void DisplayClientCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->ReclaimResources(resources);
}

void DisplayClientCompositorFrameSink::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  // TODO(fsamuel, staraz): Implement this.
}

void DisplayClientCompositorFrameSink::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  DCHECK(thread_checker_->CalledOnValidThread());
  compositor_frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

void DisplayClientCompositorFrameSink::OnDidFinishFrame(
    const cc::BeginFrameAck& ack) {
  // If there was damage, the submitted CompositorFrame includes the ack.
  if (!ack.has_damage)
    compositor_frame_sink_->BeginFrameDidNotSwap(ack);
}

}  // namespace ws
}  // namespace ui
