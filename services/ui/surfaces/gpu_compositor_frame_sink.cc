// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/gpu_compositor_frame_sink.h"

#include "services/ui/surfaces/display_compositor.h"

namespace ui {

GpuCompositorFrameSink::GpuCompositorFrameSink(
    DisplayCompositor* display_compositor,
    const cc::FrameSinkId& frame_sink_id,
    std::unique_ptr<cc::Display> display,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : display_compositor_(display_compositor),
      support_(this,
               display_compositor->manager(),
               frame_sink_id,
               std::move(display),
               std::move(begin_frame_source)),
      client_(std::move(client)),
      binding_(this, std::move(request)),
      private_binding_(this, std::move(private_request)) {
  binding_.set_connection_error_handler(base::Bind(
      &GpuCompositorFrameSink::OnClientConnectionLost, base::Unretained(this)));

  private_binding_.set_connection_error_handler(
      base::Bind(&GpuCompositorFrameSink::OnPrivateConnectionLost,
                 base::Unretained(this)));
}

GpuCompositorFrameSink::~GpuCompositorFrameSink() {}

void GpuCompositorFrameSink::EvictFrame() {
  support_.EvictFrame();
}

void GpuCompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_.SetNeedsBeginFrame(needs_begin_frame);
}

void GpuCompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalFrameId& local_frame_id,
    cc::CompositorFrame frame) {
  support_.SubmitCompositorFrame(local_frame_id, std::move(frame));
}

void GpuCompositorFrameSink::AddSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  display_compositor_->AddSurfaceReferences(references);
}

void GpuCompositorFrameSink::RemoveSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  display_compositor_->RemoveSurfaceReferences(references);
}

void GpuCompositorFrameSink::Require(const cc::LocalFrameId& local_frame_id,
                                     const cc::SurfaceSequence& sequence) {
  support_.Require(local_frame_id, sequence);
}

void GpuCompositorFrameSink::Satisfy(const cc::SurfaceSequence& sequence) {
  support_.Satisfy(sequence);
}

void GpuCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (client_)
    client_->DidReceiveCompositorFrameAck();
}

void GpuCompositorFrameSink::AddChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  support_.AddChildFrameSink(child_frame_sink_id);
}

void GpuCompositorFrameSink::RemoveChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  support_.RemoveChildFrameSink(child_frame_sink_id);
}

void GpuCompositorFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void GpuCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void GpuCompositorFrameSink::WillDrawSurface() {
  if (client_)
    client_->WillDrawSurface();
}

void GpuCompositorFrameSink::OnClientConnectionLost() {
  client_connection_lost_ = true;
  // Request destruction of |this| only if both connections are lost.
  display_compositor_->OnCompositorFrameSinkClientConnectionLost(
      support_.frame_sink_id(), private_connection_lost_);
}

void GpuCompositorFrameSink::OnPrivateConnectionLost() {
  private_connection_lost_ = true;
  // Request destruction of |this| only if both connections are lost.
  display_compositor_->OnCompositorFrameSinkPrivateConnectionLost(
      support_.frame_sink_id(), client_connection_lost_);
}

}  // namespace ui
