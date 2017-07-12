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
    ServerWindow* window)
    : window_(window) {}

ServerWindowCompositorFrameSinkManager::
    ~ServerWindowCompositorFrameSinkManager() {
}

void ServerWindowCompositorFrameSinkManager::CreateRootCompositorFrameSink(
    gfx::AcceleratedWidget widget,
    cc::mojom::CompositorFrameSinkAssociatedRequest sink_request,
    cc::mojom::CompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_request) {
  if (!pending_compositor_frame_sink_request_.is_pending()) {
    pending_compositor_frame_sink_request_ =
        mojo::MakeRequest(&compositor_frame_sink_);
  }

  // TODO(fsamuel): AcceleratedWidget cannot be transported over IPC for Mac
  // or Android. We should instead use GpuSurfaceTracker here on those
  // platforms.
  window_->delegate()->GetFrameSinkManager()->CreateRootCompositorFrameSink(
      window_->frame_sink_id(), widget, std::move(sink_request),
      std::move(pending_compositor_frame_sink_request_), std::move(client),
      std::move(display_request));
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    cc::mojom::CompositorFrameSinkRequest request,
    cc::mojom::CompositorFrameSinkClientPtr client) {
  if (!pending_compositor_frame_sink_request_.is_pending()) {
    pending_compositor_frame_sink_request_ =
        mojo::MakeRequest(&compositor_frame_sink_);
  }

  window_->delegate()->GetFrameSinkManager()->CreateCompositorFrameSink(
      window_->frame_sink_id(), std::move(request),
      std::move(pending_compositor_frame_sink_request_), std::move(client));
}

void ServerWindowCompositorFrameSinkManager::ClaimTemporaryReference(
    const viz::SurfaceId& surface_id) {
  if (!compositor_frame_sink_.is_bound()) {
    pending_compositor_frame_sink_request_ =
        mojo::MakeRequest(&compositor_frame_sink_);
  }
  compositor_frame_sink_->ClaimTemporaryReference(surface_id);
}

}  // namespace ws
}  // namespace ui
