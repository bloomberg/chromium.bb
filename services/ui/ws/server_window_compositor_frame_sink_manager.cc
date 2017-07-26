// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {
namespace ws {

ServerWindowCompositorFrameSinkManager::ServerWindowCompositorFrameSinkManager(
    const viz::FrameSinkId& frame_sink_id,
    cc::mojom::FrameSinkManager* frame_sink_manager)
    : frame_sink_id_(frame_sink_id), frame_sink_manager_(frame_sink_manager) {
  frame_sink_manager_->RegisterFrameSinkId(frame_sink_id_);
}

ServerWindowCompositorFrameSinkManager::
    ~ServerWindowCompositorFrameSinkManager() {
  frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void ServerWindowCompositorFrameSinkManager::CreateRootCompositorFrameSink(
    gfx::AcceleratedWidget widget,
    cc::mojom::CompositorFrameSinkAssociatedRequest sink_request,
    cc::mojom::CompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_request) {
  // TODO(fsamuel): AcceleratedWidget cannot be transported over IPC for Mac
  // or Android. We should instead use GpuSurfaceTracker here on those
  // platforms.
  frame_sink_manager_->CreateRootCompositorFrameSink(
      frame_sink_id_, widget, std::move(sink_request), std::move(client),
      std::move(display_request));
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    cc::mojom::CompositorFrameSinkRequest request,
    cc::mojom::CompositorFrameSinkClientPtr client) {
  frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id_, std::move(request), std::move(client));
}

void ServerWindowCompositorFrameSinkManager::ClaimTemporaryReference(
    const viz::SurfaceId& surface_id) {
  frame_sink_manager_->AssignTemporaryReference(surface_id, frame_sink_id_);
}

}  // namespace ws
}  // namespace ui
