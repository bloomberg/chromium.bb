// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_root.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ws/client_change.h"
#include "services/ws/client_change_tracker.h"
#include "services/ws/proxy_window.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura_extra/window_position_in_root_monitor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ws {

ClientRoot::ClientRoot(WindowTree* window_tree,
                       aura::Window* window,
                       bool is_top_level)
    : window_tree_(window_tree), window_(window), is_top_level_(is_top_level) {
  window_->AddObserver(this);
  if (window_->GetHost())
    window->GetHost()->AddObserver(this);
  client_surface_embedder_ = std::make_unique<aura::ClientSurfaceEmbedder>(
      window_, is_top_level, gfx::Insets());
  // Ensure there is a valid LocalSurfaceId (if necessary).
  UpdateLocalSurfaceIdIfNecessary();
  if (!is_top_level) {
    root_position_monitor_ =
        std::make_unique<aura_extra::WindowPositionInRootMonitor>(
            window, base::BindRepeating(&ClientRoot::OnPositionInRootChanged,
                                        base::Unretained(this)));
  }
}

ClientRoot::~ClientRoot() {
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  window_->RemoveObserver(this);
  if (window_->GetHost())
    window_->GetHost()->RemoveObserver(this);

  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  host_frame_sink_manager->InvalidateFrameSinkId(proxy_window->frame_sink_id());
}

void ClientRoot::SetClientAreaInsets(const gfx::Insets& client_area_insets) {
  if (!is_top_level_)
    return;

  client_surface_embedder_->SetClientAreaInsets(client_area_insets);
}

void ClientRoot::RegisterVizEmbeddingSupport() {
  // This function should only be called once.
  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  viz::FrameSinkId frame_sink_id =
      ProxyWindow::GetMayBeNull(window_)->frame_sink_id();
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id, this, viz::ReportFirstSurfaceActivation::kYes);
  window_->SetEmbedFrameSinkId(frame_sink_id);

  UpdatePrimarySurfaceId();
}

bool ClientRoot::ShouldAssignLocalSurfaceId() {
  // First level embeddings have their LocalSurfaceId assigned by the
  // WindowService. First level embeddings have no embeddings above them.
  if (is_top_level_)
    return true;
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  return proxy_window->owning_window_tree() == nullptr;
}

void ClientRoot::UpdateLocalSurfaceIdIfNecessary() {
  if (!ShouldAssignLocalSurfaceId())
    return;

  gfx::Size size_in_pixels =
      ui::ConvertSizeToPixel(window_->layer(), window_->bounds().size());
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  // It's expected by cc code that any time the size changes a new
  // LocalSurfaceId is used.
  if (last_surface_size_in_pixels_ != size_in_pixels ||
      !proxy_window->local_surface_id().has_value() ||
      last_device_scale_factor_ != window_->layer()->device_scale_factor()) {
    window_->AllocateLocalSurfaceId();
    proxy_window->set_local_surface_id(
        window_->GetLocalSurfaceIdAllocation().local_surface_id());
    last_surface_size_in_pixels_ = size_in_pixels;
    last_device_scale_factor_ = window_->layer()->device_scale_factor();
  }
}

void ClientRoot::OnLocalSurfaceIdChanged() {
  if (ShouldAssignLocalSurfaceId())
    return;

  HandleBoundsOrScaleFactorChange(is_top_level_ ? window_->GetBoundsInScreen()
                                                : window_->bounds());
}

void ClientRoot::AttachChildFrameSinkId(ProxyWindow* proxy_window) {
  DCHECK(proxy_window->attached_frame_sink_id().is_valid());
  DCHECK(ProxyWindow::GetMayBeNull(window_)->frame_sink_id().is_valid());
  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  const viz::FrameSinkId& frame_sink_id =
      proxy_window->attached_frame_sink_id();
  if (host_frame_sink_manager->IsFrameSinkIdRegistered(frame_sink_id)) {
    host_frame_sink_manager->RegisterFrameSinkHierarchy(
        ProxyWindow::GetMayBeNull(window_)->frame_sink_id(), frame_sink_id);
  }
}

void ClientRoot::UnattachChildFrameSinkId(ProxyWindow* proxy_window) {
  DCHECK(proxy_window->attached_frame_sink_id().is_valid());
  DCHECK(ProxyWindow::GetMayBeNull(window_)->frame_sink_id().is_valid());
  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  const viz::FrameSinkId& root_frame_sink_id =
      ProxyWindow::GetMayBeNull(window_)->frame_sink_id();
  const viz::FrameSinkId& window_frame_sink_id =
      proxy_window->attached_frame_sink_id();
  if (host_frame_sink_manager->IsFrameSinkHierarchyRegistered(
          root_frame_sink_id, window_frame_sink_id)) {
    host_frame_sink_manager->UnregisterFrameSinkHierarchy(root_frame_sink_id,
                                                          window_frame_sink_id);
  }
}

void ClientRoot::AttachChildFrameSinkIdRecursive(ProxyWindow* proxy_window) {
  if (proxy_window->attached_frame_sink_id().is_valid())
    AttachChildFrameSinkId(proxy_window);

  for (aura::Window* child : proxy_window->window()->children()) {
    ProxyWindow* child_proxy_window = ProxyWindow::GetMayBeNull(child);
    if (child_proxy_window->owning_window_tree() == window_tree_)
      AttachChildFrameSinkIdRecursive(child_proxy_window);
  }
}

void ClientRoot::UnattachChildFrameSinkIdRecursive(ProxyWindow* proxy_window) {
  if (proxy_window->attached_frame_sink_id().is_valid())
    UnattachChildFrameSinkId(proxy_window);

  for (aura::Window* child : proxy_window->window()->children()) {
    ProxyWindow* child_proxy_window = ProxyWindow::GetMayBeNull(child);
    if (child_proxy_window->owning_window_tree() == window_tree_)
      UnattachChildFrameSinkIdRecursive(child_proxy_window);
  }
}

void ClientRoot::UpdatePrimarySurfaceId() {
  UpdateLocalSurfaceIdIfNecessary();
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  if (proxy_window->local_surface_id().has_value()) {
    client_surface_embedder_->SetSurfaceId(viz::SurfaceId(
        window_->GetFrameSinkId(), *proxy_window->local_surface_id()));
    if (fallback_surface_info_) {
      client_surface_embedder_->SetFallbackSurfaceInfo(*fallback_surface_info_);
      fallback_surface_info_.reset();
    }
  }
}

void ClientRoot::CheckForScaleFactorChange() {
  if (!ShouldAssignLocalSurfaceId() ||
      last_device_scale_factor_ == window_->layer()->device_scale_factor()) {
    return;
  }

  HandleBoundsOrScaleFactorChange(is_top_level_ ? window_->GetBoundsInScreen()
                                                : window_->bounds());
}

void ClientRoot::HandleBoundsOrScaleFactorChange(const gfx::Rect& old_bounds) {
  UpdatePrimarySurfaceId();
  client_surface_embedder_->UpdateSizeAndGutters();
  // See comments in WindowTree::SetWindowBoundsImpl() for details on
  // why this always notifies the client.
  NotifyClientOfNewBounds(old_bounds);
}

void ClientRoot::NotifyClientOfNewBounds(const gfx::Rect& old_bounds) {
  last_bounds_ = window_->GetBoundsInScreen();
  window_tree_->window_tree_client_->OnWindowBoundsChanged(
      window_tree_->TransportIdForWindow(window_), old_bounds, last_bounds_,
      ProxyWindow::GetMayBeNull(window_)->local_surface_id());
}

void ClientRoot::OnPositionInRootChanged() {
  DCHECK(!is_top_level_);
  gfx::Rect bounds_in_screen = window_->GetBoundsInScreen();
  if (bounds_in_screen.origin() != last_bounds_.origin())
    NotifyClientOfNewBounds(last_bounds_);
}

void ClientRoot::OnWindowPropertyChanged(aura::Window* window,
                                         const void* key,
                                         intptr_t old) {
  if (window_tree_->property_change_tracker_
          ->IsProcessingPropertyChangeForWindow(window, key)) {
    // Do not send notifications for changes intiated by the client.
    return;
  }
  std::string transport_name;
  std::unique_ptr<std::vector<uint8_t>> transport_value;
  if (window_tree_->window_service()
          ->property_converter()
          ->ConvertPropertyForTransport(window, key, &transport_name,
                                        &transport_value)) {
    base::Optional<std::vector<uint8_t>> transport_value_mojo;
    if (transport_value)
      transport_value_mojo.emplace(std::move(*transport_value));
    window_tree_->window_tree_client_->OnWindowSharedPropertyChanged(
        window_tree_->TransportIdForWindow(window), transport_name,
        transport_value_mojo);
  }
}

void ClientRoot::OnWindowBoundsChanged(aura::Window* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds,
                                       ui::PropertyChangeReason reason) {
  if (!is_top_level_) {
    HandleBoundsOrScaleFactorChange(old_bounds);
    return;
  }
  gfx::Rect old_bounds_in_screen = old_bounds;
  aura::Window* root = window->GetRootWindow();
  if (root && aura::client::GetScreenPositionClient(root))
    ::wm::ConvertRectToScreen(window->parent(), &old_bounds_in_screen);
  if (is_moving_across_displays_) {
    if (!scheduled_change_old_bounds_)
      scheduled_change_old_bounds_ = old_bounds_in_screen;
    return;
  }
  DCHECK(!scheduled_change_old_bounds_);
  HandleBoundsOrScaleFactorChange(old_bounds_in_screen);
}

void ClientRoot::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, window_);
  DCHECK(window->GetHost());
  window->GetHost()->AddObserver(this);
  window_tree_->window_tree_client_->OnWindowDisplayChanged(
      window_tree_->TransportIdForWindow(window),
      window->GetHost()->GetDisplayId());

  // When the addition to a new root window isn't the result of moving across
  // displays (e.g. destruction of the current display), the window bounds in
  // screen change even though its bounds in the root window remain the same.
  if (is_top_level_ && !is_moving_across_displays_)
    HandleBoundsOrScaleFactorChange(window->GetBoundsInScreen());
  else
    CheckForScaleFactorChange();
}

void ClientRoot::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                aura::Window* new_root) {
  DCHECK_EQ(window, window_);
  DCHECK(window->GetHost());
  window->GetHost()->RemoveObserver(this);
}

void ClientRoot::OnWillMoveWindowToDisplay(aura::Window* window,
                                           int64_t new_display_id) {
  DCHECK(!is_moving_across_displays_);
  is_moving_across_displays_ = true;
}

void ClientRoot::OnDidMoveWindowToDisplay(aura::Window* window) {
  DCHECK(is_moving_across_displays_);
  is_moving_across_displays_ = false;
  if (scheduled_change_old_bounds_) {
    HandleBoundsOrScaleFactorChange(scheduled_change_old_bounds_.value());
    scheduled_change_old_bounds_.reset();
  }
}

void ClientRoot::OnHostResized(aura::WindowTreeHost* host) {
  // This function is also called when the device-scale-factor changes too.
  CheckForScaleFactorChange();
}

void ClientRoot::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  if (proxy_window->local_surface_id().has_value()) {
    DCHECK(!fallback_surface_info_);
    if (!client_surface_embedder_->HasPrimarySurfaceId())
      UpdatePrimarySurfaceId();
    client_surface_embedder_->SetFallbackSurfaceInfo(surface_info);
  } else {
    fallback_surface_info_ = std::make_unique<viz::SurfaceInfo>(surface_info);
  }
  if (!window_tree_->client_name().empty()) {
    // OnFirstSurfaceActivation() should only be called after
    // AttachCompositorFrameSink().
    DCHECK(proxy_window->attached_compositor_frame_sink());
    window_tree_->window_service()->OnFirstSurfaceActivation(
        window_tree_->client_name());
  }
}

void ClientRoot::OnFrameTokenChanged(uint32_t frame_token) {
  // TODO: this needs to call through to WindowTreeClient.
  // https://crbug.com/848022.
}

}  // namespace ws
