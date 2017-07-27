// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/frame_sink_manager_client_binding.h"

#include "services/ui/ws/gpu_host.h"

namespace ui {
namespace ws {

FrameSinkManagerClientBinding::FrameSinkManagerClientBinding(
    viz::mojom::FrameSinkManagerClient* frame_sink_manager_client,
    GpuHost* gpu_host)
    : frame_sink_manager_client_binding_(frame_sink_manager_client) {
  viz::mojom::FrameSinkManagerClientPtr client_proxy;
  frame_sink_manager_client_binding_.Bind(mojo::MakeRequest(&client_proxy));
  gpu_host->CreateFrameSinkManager(mojo::MakeRequest(&frame_sink_manager_),
                                   std::move(client_proxy));
}

FrameSinkManagerClientBinding::~FrameSinkManagerClientBinding() = default;

void FrameSinkManagerClientBinding::RegisterFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_manager_->RegisterFrameSinkId(frame_sink_id);
}

void FrameSinkManagerClientBinding::InvalidateFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id);
}

void FrameSinkManagerClientBinding::CreateRootCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    viz::mojom::CompositorFrameSinkAssociatedRequest request,
    viz::mojom::CompositorFrameSinkClientPtr client,
    viz::mojom::DisplayPrivateAssociatedRequest display_private_request) {
  frame_sink_manager_->CreateRootCompositorFrameSink(
      frame_sink_id, surface_handle, std::move(request), std::move(client),
      std::move(display_private_request));
}

void FrameSinkManagerClientBinding::CreateCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    viz::mojom::CompositorFrameSinkRequest request,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id, std::move(request), std::move(client));
}

void FrameSinkManagerClientBinding::RegisterFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  frame_sink_manager_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                  child_frame_sink_id);
}

void FrameSinkManagerClientBinding::UnregisterFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  frame_sink_manager_->UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                    child_frame_sink_id);
}

void FrameSinkManagerClientBinding::AssignTemporaryReference(
    const viz::SurfaceId& surface_id,
    const viz::FrameSinkId& owner) {
  frame_sink_manager_->AssignTemporaryReference(surface_id, owner);
}

void FrameSinkManagerClientBinding::DropTemporaryReference(
    const viz::SurfaceId& surface_id) {
  frame_sink_manager_->DropTemporaryReference(surface_id);
}

}  // namespace ws
}  // namespace ui
