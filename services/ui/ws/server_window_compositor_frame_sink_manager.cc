// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

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
  if (!frame_sink_data_)
    frame_sink_data_ = base::MakeUnique<CompositorFrameSinkData>();

  cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request;
  if (frame_sink_data_->pending_compositor_frame_sink_request.is_pending()) {
    private_request =
        std::move(frame_sink_data_->pending_compositor_frame_sink_request);
  } else {
    private_request =
        mojo::MakeRequest(&frame_sink_data_->compositor_frame_sink);
  }

  // TODO(fsamuel): AcceleratedWidget cannot be transported over IPC for Mac
  // or Android. We should instead use GpuSurfaceTracker here on those
  // platforms.
  window_->delegate()->GetDisplayCompositor()->CreateRootCompositorFrameSink(
      window_->frame_sink_id(), widget, std::move(sink_request),
      std::move(private_request), std::move(client),
      std::move(display_request));
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  if (!frame_sink_data_)
    frame_sink_data_ = base::MakeUnique<CompositorFrameSinkData>();

  cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request;
  if (frame_sink_data_->pending_compositor_frame_sink_request.is_pending()) {
    private_request =
        std::move(frame_sink_data_->pending_compositor_frame_sink_request);
  } else {
    private_request =
        mojo::MakeRequest(&frame_sink_data_->compositor_frame_sink);
  }

  window_->delegate()->GetDisplayCompositor()->CreateCompositorFrameSink(
      window_->frame_sink_id(), std::move(request), std::move(private_request),
      std::move(client));
}

CompositorFrameSinkData::CompositorFrameSinkData() {}

CompositorFrameSinkData::~CompositorFrameSinkData() {}

CompositorFrameSinkData::CompositorFrameSinkData(
    CompositorFrameSinkData&& other)
    : compositor_frame_sink(std::move(other.compositor_frame_sink)) {}

CompositorFrameSinkData& CompositorFrameSinkData::operator=(
    CompositorFrameSinkData&& other) {
  compositor_frame_sink = std::move(other.compositor_frame_sink);
  return *this;
}

}  // namespace ws
}  // namespace ui
