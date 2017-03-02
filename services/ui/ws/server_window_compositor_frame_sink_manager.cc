// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

#include <utility>

#include "cc/ipc/display_compositor.mojom.h"
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
    cc::mojom::MojoCompositorFrameSinkAssociatedRequest sink_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_request) {
  if (!pending_compositor_frame_sink_request_.is_pending()) {
    pending_compositor_frame_sink_request_ =
        mojo::MakeRequest(&compositor_frame_sink_);
  }

  // TODO(fsamuel): AcceleratedWidget cannot be transported over IPC for Mac
  // or Android. We should instead use GpuSurfaceTracker here on those
  // platforms.
  window_->delegate()->GetDisplayCompositor()->CreateRootCompositorFrameSink(
      window_->frame_sink_id(), widget, std::move(sink_request),
      std::move(pending_compositor_frame_sink_request_), std::move(client),
      std::move(display_request));
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  if (!pending_compositor_frame_sink_request_.is_pending()) {
    pending_compositor_frame_sink_request_ =
        mojo::MakeRequest(&compositor_frame_sink_);
  }

  window_->delegate()->GetDisplayCompositor()->CreateCompositorFrameSink(
      window_->frame_sink_id(), std::move(request),
      std::move(pending_compositor_frame_sink_request_), std::move(client));
}

void ServerWindowCompositorFrameSinkManager::ClaimTemporaryReference(
    const cc::SurfaceId& surface_id) {
  if (!compositor_frame_sink_.is_bound()) {
    pending_compositor_frame_sink_request_ =
        mojo::MakeRequest(&compositor_frame_sink_);
  }
  compositor_frame_sink_->ClaimTemporaryReference(surface_id);
}

}  // namespace ws
}  // namespace ui
