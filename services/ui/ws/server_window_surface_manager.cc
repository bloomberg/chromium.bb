// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_surface_manager.h"

#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_surface.h"

namespace ui {
namespace ws {

ServerWindowSurfaceManager::ServerWindowSurfaceManager(ServerWindow* window)
    : window_(window),
      waiting_for_initial_frames_(
          window_->properties().count(ui::mojom::kWaitForUnderlay_Property) >
          0) {
}

ServerWindowSurfaceManager::~ServerWindowSurfaceManager() {
  // Explicitly clear the type to surface manager so that this manager
  // is still valid prior during ~ServerWindowSurface.
  type_to_surface_map_.clear();
}

bool ServerWindowSurfaceManager::ShouldDraw() {
  if (!waiting_for_initial_frames_)
    return true;

  waiting_for_initial_frames_ =
      !IsSurfaceReadyAndNonEmpty(mojom::SurfaceType::DEFAULT) ||
      !IsSurfaceReadyAndNonEmpty(mojom::SurfaceType::UNDERLAY);
  return !waiting_for_initial_frames_;
}

void ServerWindowSurfaceManager::CreateSurface(
    mojom::SurfaceType surface_type,
    mojo::InterfaceRequest<mojom::Surface> request,
    mojom::SurfaceClientPtr client) {
  cc::FrameSinkId frame_sink_id(WindowIdToTransportId(window_->id()),
                                static_cast<uint32_t>(surface_type));
  std::unique_ptr<ServerWindowSurface> surface(new ServerWindowSurface(
      this, frame_sink_id, std::move(request), std::move(client)));
  type_to_surface_map_[surface_type] = std::move(surface);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetDefaultSurface() const {
  return GetSurfaceByType(mojom::SurfaceType::DEFAULT);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetUnderlaySurface() const {
  return GetSurfaceByType(mojom::SurfaceType::UNDERLAY);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetSurfaceByType(
    mojom::SurfaceType type) const {
  auto iter = type_to_surface_map_.find(type);
  return iter == type_to_surface_map_.end() ? nullptr : iter->second.get();
}

bool ServerWindowSurfaceManager::HasSurfaceOfType(
    mojom::SurfaceType type) const {
  return type_to_surface_map_.count(type) > 0;
}

bool ServerWindowSurfaceManager::HasAnySurface() const {
  return GetDefaultSurface() || GetUnderlaySurface();
}

cc::SurfaceManager* ServerWindowSurfaceManager::GetSurfaceManager() {
  return window()->delegate()->GetDisplayCompositor()->manager();
}

bool ServerWindowSurfaceManager::IsSurfaceReadyAndNonEmpty(
    mojom::SurfaceType type) const {
  auto iter = type_to_surface_map_.find(type);
  if (iter == type_to_surface_map_.end())
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
