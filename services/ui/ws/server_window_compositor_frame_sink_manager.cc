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

void ServerWindowCompositorFrameSinkManager::CreateDisplayCompositorFrameSink(
    gfx::AcceleratedWidget widget,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateRequest display_private_request) {
  CreateCompositorFrameSinkInternal(widget, std::move(request),
                                    std::move(client),
                                    std::move(display_private_request));
}

void ServerWindowCompositorFrameSinkManager::CreateOffscreenCompositorFrameSink(
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  CreateCompositorFrameSinkInternal(gfx::kNullAcceleratedWidget,
                                    std::move(request), std::move(client),
                                    nullptr);
}

void ServerWindowCompositorFrameSinkManager::AddChildFrameSinkId(
    const cc::FrameSinkId& frame_sink_id) {
  if (frame_sink_data_) {
    frame_sink_data_->compositor_frame_sink->AddChildFrameSink(frame_sink_id);
    return;
  }
  frame_sink_data_ = base::MakeUnique<CompositorFrameSinkData>();
  frame_sink_data_->pending_compositor_frame_sink_request =
      mojo::MakeRequest(&frame_sink_data_->compositor_frame_sink);
  frame_sink_data_->compositor_frame_sink->AddChildFrameSink(frame_sink_id);
}

void ServerWindowCompositorFrameSinkManager::RemoveChildFrameSinkId(
    const cc::FrameSinkId& frame_sink_id) {
  DCHECK(frame_sink_data_);
  frame_sink_data_->compositor_frame_sink->RemoveChildFrameSink(frame_sink_id);
}

bool ServerWindowCompositorFrameSinkManager::HasCompositorFrameSink() const {
  return !!frame_sink_data_;
}

gfx::Size ServerWindowCompositorFrameSinkManager::GetLatestFrameSize() const {
  if (!frame_sink_data_)
    return gfx::Size();

  return frame_sink_data_->latest_submitted_surface_info.size_in_pixels();
}

cc::SurfaceId ServerWindowCompositorFrameSinkManager::GetLatestSurfaceId()
    const {
  if (!frame_sink_data_)
    return cc::SurfaceId();

  return frame_sink_data_->latest_submitted_surface_info.id();
}

void ServerWindowCompositorFrameSinkManager::SetLatestSurfaceInfo(
    const cc::SurfaceInfo& surface_info) {
  if (!frame_sink_data_)
    frame_sink_data_ = base::MakeUnique<CompositorFrameSinkData>();

  frame_sink_data_->latest_submitted_surface_info = surface_info;
}

void ServerWindowCompositorFrameSinkManager::OnRootChanged(
    ServerWindow* old_root,
    ServerWindow* new_root) {
  if (!frame_sink_data_)
    return;

  if (old_root) {
    old_root->GetOrCreateCompositorFrameSinkManager()->RemoveChildFrameSinkId(
        frame_sink_data_->frame_sink_id);
  }
  if (new_root) {
    new_root->GetOrCreateCompositorFrameSinkManager()->AddChildFrameSinkId(
        frame_sink_data_->frame_sink_id);
  }
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSinkInternal(
    gfx::AcceleratedWidget widget,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateRequest display_private_request) {
  cc::FrameSinkId frame_sink_id(WindowIdToTransportId(window_->id()), 0);

  if (!frame_sink_data_)
    frame_sink_data_ = base::MakeUnique<CompositorFrameSinkData>();

  frame_sink_data_->frame_sink_id = frame_sink_id;
  cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request;
  if (frame_sink_data_->pending_compositor_frame_sink_request.is_pending()) {
    private_request =
        std::move(frame_sink_data_->pending_compositor_frame_sink_request);
  } else {
    private_request =
        mojo::MakeRequest(&frame_sink_data_->compositor_frame_sink);
  }
  if (widget != gfx::kNullAcceleratedWidget) {
    // TODO(fsamuel): AcceleratedWidget cannot be transported over IPC for Mac
    // or Android. We should instead use GpuSurfaceTracker here on those
    // platforms.
    window_->delegate()
        ->GetDisplayCompositor()
        ->CreateDisplayCompositorFrameSink(
            frame_sink_id, widget, std::move(request),
            std::move(private_request), std::move(client),
            std::move(display_private_request));
  } else {
    DCHECK(display_private_request.Equals(nullptr));
    window_->delegate()
        ->GetDisplayCompositor()
        ->CreateOffscreenCompositorFrameSink(frame_sink_id, std::move(request),
                                             std::move(private_request),
                                             std::move(client));
  }

  if (window_->parent()) {
    ServerWindow* root_window = window_->GetRoot();
    if (root_window) {
      root_window->GetOrCreateCompositorFrameSinkManager()->AddChildFrameSinkId(
          frame_sink_id);
    }
  }
}

CompositorFrameSinkData::CompositorFrameSinkData() {}

CompositorFrameSinkData::~CompositorFrameSinkData() {}

CompositorFrameSinkData::CompositorFrameSinkData(
    CompositorFrameSinkData&& other)
    : latest_submitted_surface_info(other.latest_submitted_surface_info),
      compositor_frame_sink(std::move(other.compositor_frame_sink)) {}

CompositorFrameSinkData& CompositorFrameSinkData::operator=(
    CompositorFrameSinkData&& other) {
  latest_submitted_surface_info = other.latest_submitted_surface_info;
  compositor_frame_sink = std::move(other.compositor_frame_sink);
  return *this;
}

}  // namespace ws
}  // namespace ui
