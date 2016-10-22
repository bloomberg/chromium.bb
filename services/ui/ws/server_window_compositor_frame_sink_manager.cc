// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {
namespace ws {

ServerWindowCompositorFrameSinkManager::ServerWindowCompositorFrameSinkManager(
    ServerWindow* window)
    : window_(window),
      waiting_for_initial_frames_(
          window_->properties().count(ui::mojom::kWaitForUnderlay_Property) >
          0) {}

ServerWindowCompositorFrameSinkManager::
    ~ServerWindowCompositorFrameSinkManager() {
  // Explicitly clear the type to surface manager so that this manager
  // is still valid prior during ~ServerWindowCompositorFrameSink.
  type_to_compositor_frame_sink_map_.clear();
}

bool ServerWindowCompositorFrameSinkManager::ShouldDraw() {
  if (!waiting_for_initial_frames_)
    return true;

  waiting_for_initial_frames_ = !IsCompositorFrameSinkReadyAndNonEmpty(
                                    mojom::CompositorFrameSinkType::DEFAULT) ||
                                !IsCompositorFrameSinkReadyAndNonEmpty(
                                    mojom::CompositorFrameSinkType::UNDERLAY);
  return !waiting_for_initial_frames_;
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink> request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  cc::FrameSinkId frame_sink_id(
      WindowIdToTransportId(window_->id()),
      static_cast<uint32_t>(compositor_frame_sink_type));
  std::unique_ptr<ServerWindowCompositorFrameSink> compositor_frame_sink(
      new ServerWindowCompositorFrameSink(
          this, frame_sink_id, std::move(request), std::move(client)));
  type_to_compositor_frame_sink_map_[compositor_frame_sink_type] =
      std::move(compositor_frame_sink);
}

ServerWindowCompositorFrameSink*
ServerWindowCompositorFrameSinkManager::GetDefaultCompositorFrameSink() const {
  return GetCompositorFrameSinkByType(mojom::CompositorFrameSinkType::DEFAULT);
}

ServerWindowCompositorFrameSink*
ServerWindowCompositorFrameSinkManager::GetUnderlayCompositorFrameSink() const {
  return GetCompositorFrameSinkByType(mojom::CompositorFrameSinkType::UNDERLAY);
}

ServerWindowCompositorFrameSink*
ServerWindowCompositorFrameSinkManager::GetCompositorFrameSinkByType(
    mojom::CompositorFrameSinkType type) const {
  auto iter = type_to_compositor_frame_sink_map_.find(type);
  return iter == type_to_compositor_frame_sink_map_.end() ? nullptr
                                                          : iter->second.get();
}

bool ServerWindowCompositorFrameSinkManager::HasCompositorFrameSinkOfType(
    mojom::CompositorFrameSinkType type) const {
  return type_to_compositor_frame_sink_map_.count(type) > 0;
}

bool ServerWindowCompositorFrameSinkManager::HasAnyCompositorFrameSink() const {
  return GetDefaultCompositorFrameSink() || GetUnderlayCompositorFrameSink();
}

cc::SurfaceManager*
ServerWindowCompositorFrameSinkManager::GetCompositorFrameSinkManager() {
  return window()->delegate()->GetDisplayCompositor()->manager();
}

bool ServerWindowCompositorFrameSinkManager::
    IsCompositorFrameSinkReadyAndNonEmpty(
        mojom::CompositorFrameSinkType type) const {
  auto iter = type_to_compositor_frame_sink_map_.find(type);
  if (iter == type_to_compositor_frame_sink_map_.end())
    return false;
  if (iter->second->last_submitted_frame_size().IsEmpty())
    return false;
  const gfx::Size& last_submitted_frame_size =
      iter->second->last_submitted_frame_size();
  return last_submitted_frame_size.width() >= window_->bounds().width() &&
         last_submitted_frame_size.height() >= window_->bounds().height();
}

}  // namespace ws
}  // namespace ui
