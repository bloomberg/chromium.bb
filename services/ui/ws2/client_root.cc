// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/client_root.h"

#include "services/ui/ws2/client_change.h"
#include "services/ui/ws2/client_change_tracker.h"
#include "services/ui/ws2/window_data.h"
#include "services/ui/ws2/window_service_client.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/window.h"
#include "ui/compositor/dip_util.h"

namespace ui {
namespace ws2 {

ClientRoot::ClientRoot(WindowServiceClient* window_service_client,
                       aura::Window* window,
                       bool is_top_level)
    : window_service_client_(window_service_client),
      window_(window),
      is_top_level_(is_top_level) {
  WindowData::GetMayBeNull(window)->set_embedded_window_service_client(
      window_service_client);
  window_->AddObserver(this);
  // TODO: wire up gfx::Insets() correctly below. See usage in
  // aura::ClientSurfaceEmbedder for details. Insets here are used for
  // guttering.
  // TODO: this may only be needed for top-level windows and any ClientRoots
  // created as the result of a direct call to create a WindowServiceClient by
  // code running in process (that is, not at the request of a client through
  // WindowServiceClient).
  client_surface_embedder_ = std::make_unique<aura::ClientSurfaceEmbedder>(
      window_, is_top_level, gfx::Insets());
}

ClientRoot::~ClientRoot() {
  WindowData::GetMayBeNull(window_)->set_embedded_window_service_client(
      nullptr);
  window_->RemoveObserver(this);
}

void ClientRoot::FrameSinkIdChanged() {
  window_->SetEmbedFrameSinkId(
      WindowData::GetMayBeNull(window_)->frame_sink_id());
  UpdatePrimarySurfaceId();
}

const viz::LocalSurfaceId& ClientRoot::GetLocalSurfaceId() {
  gfx::Size size_in_pixels =
      ui::ConvertSizeToPixel(window_->layer(), window_->bounds().size());
  // It's expected by cc code that any time the size changes a new
  // LocalSurfaceId is used.
  if (last_surface_size_in_pixels_ != size_in_pixels ||
      !local_surface_id_.is_valid()) {
    local_surface_id_ = parent_local_surface_id_allocator_.GenerateId();
    last_surface_size_in_pixels_ = size_in_pixels;
  }
  return local_surface_id_;
}

void ClientRoot::UpdatePrimarySurfaceId() {
  client_surface_embedder_->SetPrimarySurfaceId(
      viz::SurfaceId(window_->GetFrameSinkId(), GetLocalSurfaceId()));
}

void ClientRoot::OnWindowBoundsChanged(aura::Window* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds,
                                       ui::PropertyChangeReason reason) {
  UpdatePrimarySurfaceId();
  client_surface_embedder_->UpdateSizeAndGutters();
  base::Optional<viz::LocalSurfaceId> surface_id = local_surface_id_;
  if (window_service_client_->property_change_tracker_
          ->IsProcessingChangeForWindow(window, ClientChangeType::kBounds)) {
    // The expectation is the client is not notified of changes the client
    // initiated.
    return;
  }
  window_service_client_->window_tree_client_->OnWindowBoundsChanged(
      window_service_client_->TransportIdForWindow(window), old_bounds,
      new_bounds, std::move(surface_id));
}

}  // namespace ws2
}  // namespace ui
