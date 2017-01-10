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
  CreateCompositorFrameSinkInternal(
      mojom::CompositorFrameSinkType::DEFAULT, widget, std::move(request),
      std::move(client), std::move(display_private_request));
}

void ServerWindowCompositorFrameSinkManager::CreateOffscreenCompositorFrameSink(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  CreateCompositorFrameSinkInternal(
      compositor_frame_sink_type, gfx::kNullAcceleratedWidget,
      std::move(request), std::move(client), nullptr);
}

void ServerWindowCompositorFrameSinkManager::AddChildFrameSinkId(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    const cc::FrameSinkId& frame_sink_id) {
  auto it = type_to_compositor_frame_sink_map_.find(compositor_frame_sink_type);
  if (it != type_to_compositor_frame_sink_map_.end()) {
    it->second.compositor_frame_sink->AddChildFrameSink(frame_sink_id);
    return;
  }
  CompositorFrameSinkData& data =
      type_to_compositor_frame_sink_map_[compositor_frame_sink_type];
  data.pending_compositor_frame_sink_request =
      mojo::MakeRequest(&data.compositor_frame_sink);
  data.compositor_frame_sink->AddChildFrameSink(frame_sink_id);
}

void ServerWindowCompositorFrameSinkManager::RemoveChildFrameSinkId(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    const cc::FrameSinkId& frame_sink_id) {
  auto it = type_to_compositor_frame_sink_map_.find(compositor_frame_sink_type);
  DCHECK(it != type_to_compositor_frame_sink_map_.end());
  it->second.compositor_frame_sink->RemoveChildFrameSink(frame_sink_id);
}

bool ServerWindowCompositorFrameSinkManager::HasCompositorFrameSinkOfType(
    mojom::CompositorFrameSinkType type) const {
  return type_to_compositor_frame_sink_map_.count(type) > 0;
}

bool ServerWindowCompositorFrameSinkManager::HasAnyCompositorFrameSink() const {
  return HasCompositorFrameSinkOfType(
             mojom::CompositorFrameSinkType::DEFAULT) ||
         HasCompositorFrameSinkOfType(mojom::CompositorFrameSinkType::UNDERLAY);
}

gfx::Size ServerWindowCompositorFrameSinkManager::GetLatestFrameSize(
    mojom::CompositorFrameSinkType type) const {
  auto it = type_to_compositor_frame_sink_map_.find(type);
  if (it == type_to_compositor_frame_sink_map_.end())
    return gfx::Size();

  return it->second.latest_submitted_surface_info.size_in_pixels();
}

cc::SurfaceId ServerWindowCompositorFrameSinkManager::GetLatestSurfaceId(
    mojom::CompositorFrameSinkType type) const {
  auto it = type_to_compositor_frame_sink_map_.find(type);
  if (it == type_to_compositor_frame_sink_map_.end())
    return cc::SurfaceId();

  return it->second.latest_submitted_surface_info.id();
}

void ServerWindowCompositorFrameSinkManager::SetLatestSurfaceInfo(
    mojom::CompositorFrameSinkType type,
    const cc::SurfaceInfo& surface_info) {
  CompositorFrameSinkData& data = type_to_compositor_frame_sink_map_[type];
  data.latest_submitted_surface_info = surface_info;
}

void ServerWindowCompositorFrameSinkManager::OnRootChanged(
    ServerWindow* old_root,
    ServerWindow* new_root) {
  for (const auto& pair : type_to_compositor_frame_sink_map_) {
    if (old_root) {
      old_root->GetOrCreateCompositorFrameSinkManager()->RemoveChildFrameSinkId(
          pair.first, pair.second.frame_sink_id);
    }
    if (new_root) {
      new_root->GetOrCreateCompositorFrameSinkManager()->AddChildFrameSinkId(
          pair.first, pair.second.frame_sink_id);
    }
  }
}

bool ServerWindowCompositorFrameSinkManager::
    IsCompositorFrameSinkReadyAndNonEmpty(
        mojom::CompositorFrameSinkType type) const {
  auto iter = type_to_compositor_frame_sink_map_.find(type);
  if (iter == type_to_compositor_frame_sink_map_.end())
    return false;
  if (iter->second.latest_submitted_surface_info.size_in_pixels().IsEmpty())
    return false;
  const gfx::Size& latest_submitted_frame_size =
      iter->second.latest_submitted_surface_info.size_in_pixels();
  return latest_submitted_frame_size.width() >= window_->bounds().width() &&
         latest_submitted_frame_size.height() >= window_->bounds().height();
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSinkInternal(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    gfx::AcceleratedWidget widget,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateRequest display_private_request) {
  cc::FrameSinkId frame_sink_id(
      WindowIdToTransportId(window_->id()),
      static_cast<uint32_t>(compositor_frame_sink_type));
  CompositorFrameSinkData& data =
      type_to_compositor_frame_sink_map_[compositor_frame_sink_type];
  data.frame_sink_id = frame_sink_id;
  cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request;
  if (data.pending_compositor_frame_sink_request.is_pending()) {
    private_request = std::move(data.pending_compositor_frame_sink_request);
  } else {
    private_request = mojo::MakeRequest(&data.compositor_frame_sink);
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
          mojom::CompositorFrameSinkType::DEFAULT, frame_sink_id);
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
