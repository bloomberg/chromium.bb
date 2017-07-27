// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/compositor_frame_sink_client_binding.h"

namespace ui {
namespace ws {

CompositorFrameSinkClientBinding::CompositorFrameSinkClientBinding(
    viz::mojom::CompositorFrameSinkClient* sink_client,
    viz::mojom::CompositorFrameSinkClientRequest sink_client_request,
    viz::mojom::CompositorFrameSinkAssociatedPtr compositor_frame_sink,
    viz::mojom::DisplayPrivateAssociatedPtr display_private)
    : binding_(sink_client, std::move(sink_client_request)),
      display_private_(std::move(display_private)),
      compositor_frame_sink_(std::move(compositor_frame_sink)) {}

CompositorFrameSinkClientBinding::~CompositorFrameSinkClientBinding() = default;

void CompositorFrameSinkClientBinding::SetNeedsBeginFrame(
    bool needs_begin_frame) {
  compositor_frame_sink_->SetNeedsBeginFrame(needs_begin_frame);
}

void CompositorFrameSinkClientBinding::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  if (local_surface_id != local_surface_id_) {
    local_surface_id_ = local_surface_id;
    gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
    display_private_->ResizeDisplay(frame_size);
    display_private_->SetLocalSurfaceId(local_surface_id_,
                                        frame.metadata.device_scale_factor);
  }
  compositor_frame_sink_->SubmitCompositorFrame(local_surface_id_,
                                                std::move(frame));
}

void CompositorFrameSinkClientBinding::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  compositor_frame_sink_->DidNotProduceFrame(ack);
}

}  // namespace ws
}  // namespace ui
