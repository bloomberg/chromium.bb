// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_root.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ws/client_change.h"
#include "services/ws/client_change_tracker.h"
#include "services/ws/common/switches.h"
#include "services/ws/proxy_window.h"
#include "services/ws/top_level_proxy_window.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura_extra/window_position_in_root_monitor.h"
#include "ui/base/layout.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ws {
namespace {

bool ShouldAssignLocalSurfaceIdImpl(aura::Window* window, bool is_top_level) {
  // The window service assigns LocalSurfaceIds in two cases:
  // . Top-levels. This is because the window service is the one creating the
  //   Window, and effectively embedding the client.
  // . An embedding created by a WindowTree that was not itself embedded. This
  //   scenario is similar to top-levels, where the Window is not itself
  //   embedded in another window. An example of this is the app-list embedding
  //   a Window that contains a WebContents, where the app-list runs in process
  //   (not using the window-service APIs).
  if (is_top_level)
    return true;
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window);
  return proxy_window->owning_window_tree() == nullptr;
}

// Returns the bounds of the |window| in screen coordinate, without affect of
// the gfx::Transform. aura::Window::GetBoundsInScreen() is affected by
// Transform and may return wrong origin on overview mode, which may confuse
// locations of transient clients or tooltip windows. See
// https://crbug.com/931161.
gfx::Rect GetBoundsToSend(aura::Window* window) {
  gfx::Rect bounds = window->bounds();
  // Window may not have the root window in some tests, so use the topmost
  // window in the hierarchy for root window.
  aura::Window* root = window;
  for (auto* w = window->parent(); w; w = w->parent()) {
    bounds += w->bounds().OffsetFromOrigin();
    root = w;
  }
  // Typically root window bounds should be (0, 0), but it's not on some tests.
  bounds += (root->GetBoundsInScreen().origin() - root->bounds().origin());
  return bounds;
}

// Converts the size of the window to pixels. This function must match that used
// by WindowTreeHostMus (otherwise this code may not generate a new
// LocalSurfaceId when the client believes one should be generated). See
// https://crbug.com/952095 for more details.
gfx::Size ConvertWindowSizeToPixels(aura::Window* window) {
  return gfx::ScaleToCeiledSize(window->bounds().size(),
                                window->layer()->device_scale_factor());
}

}  // namespace

ClientRoot::ClientRoot(WindowTree* window_tree,
                       aura::Window* window,
                       bool is_top_level)
    : window_tree_(window_tree),
      window_(window),
      is_top_level_(is_top_level),
      last_bounds_(GetBoundsToSend(window)),
      last_visible_(!is_top_level && window->IsVisible()),
      last_display_id_(display::kInvalidDisplayId) {
  window_->AddObserver(this);
  if (window_->GetHost())
    window->GetHost()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  client_surface_embedder_ =
      std::make_unique<aura::ClientSurfaceEmbedder>(window_);
  if (ShouldAssignLocalSurfaceIdImpl(window, is_top_level_))
    parent_local_surface_id_allocator_.emplace();
  // Ensure there is a valid LocalSurfaceId (if necessary).
  GenerateLocalSurfaceIdIfNecessary();
  if (!is_top_level) {
    root_position_monitor_ =
        std::make_unique<aura_extra::WindowPositionInRootMonitor>(
            window, base::BindRepeating(&ClientRoot::OnPositionInRootChanged,
                                        base::Unretained(this)));
  }
}

ClientRoot::~ClientRoot() {
  if (force_visible_)
    force_visible_->OnClientRootDestroyed();

  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  window_->RemoveObserver(this);
  if (window_->GetHost())
    window_->GetHost()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  host_frame_sink_manager->InvalidateFrameSinkId(proxy_window->frame_sink_id());
}

void ClientRoot::RegisterVizEmbeddingSupport() {
  // This function should only be called once.
  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  viz::FrameSinkId frame_sink_id =
      ProxyWindow::GetMayBeNull(window_)->frame_sink_id();
  // This code only needs first-surface-activation for tests.
  const bool wants_first_surface_activation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseTestConfig);
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id, this,
      wants_first_surface_activation ? viz::ReportFirstSurfaceActivation::kYes
                                     : viz::ReportFirstSurfaceActivation::kNo);
  window_->SetEmbedFrameSinkId(frame_sink_id);

  UpdateLocalSurfaceIdAndClientSurfaceEmbedder();
}

void ClientRoot::OnForceVisibleDestroyed() {
  force_visible_ = nullptr;
  NotifyClientOfVisibilityChange();
}

void ClientRoot::GenerateLocalSurfaceIdIfNecessary() {
  if (!ShouldAssignLocalSurfaceId())
    return;

  gfx::Size size_in_pixels = ConvertWindowSizeToPixels(window_);
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  // It's expected by cc code that any time the size changes a new
  // LocalSurfaceId is used.
  if (last_surface_size_in_pixels_ != size_in_pixels ||
      !proxy_window->local_surface_id_allocation().has_value() ||
      last_device_scale_factor_ != window_->layer()->device_scale_factor()) {
    parent_local_surface_id_allocator_->GenerateId();
    UpdateSurfacePropertiesCache();
  }
}

void ClientRoot::UpdateSurfacePropertiesCache() {
  ProxyWindow::GetMayBeNull(window_)->set_local_surface_id_allocation(
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceIdAllocation());
  last_device_scale_factor_ = window_->layer()->device_scale_factor();
  last_surface_size_in_pixels_ = ConvertWindowSizeToPixels(window_);
}

bool ClientRoot::SetBoundsInScreenFromClient(
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceIdAllocation>& allocation) {
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  const base::Optional<viz::LocalSurfaceIdAllocation> starting_allocation =
      proxy_window->local_surface_id_allocation();
  {
    base::AutoReset<bool> resetter(&setting_bounds_from_client_, true);
    display::Display dst_display =
        display::Screen::GetScreen()->GetDisplayMatching(bounds);
    window_->SetBoundsInScreen(bounds, dst_display);
  }
  if (allocation)
    parent_local_surface_id_allocator_->UpdateFromChild(*allocation);

  const bool needs_new_surface_id =
      !allocation || bounds.size() != window_->bounds().size();
  if (needs_new_surface_id)
    parent_local_surface_id_allocator_->GenerateId();
  UpdateSurfacePropertiesCache();

  if (starting_allocation != proxy_window->local_surface_id_allocation())
    UpdateLocalSurfaceIdAndClientSurfaceEmbedder();

  const bool succeeded = bounds == GetBoundsToSend(window_);
  // The bounds and id form a unique pair, so that if either differ from what
  // the client requested, the client needs to be notified by way of
  // NotifyClientOfNewBounds().
  if (!succeeded || needs_new_surface_id)
    NotifyClientOfNewBounds();
  return succeeded;
}

void ClientRoot::UpdateLocalSurfaceIdFromChild(
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  if (!parent_local_surface_id_allocator_->UpdateFromChild(
          local_surface_id_allocation)) {
    return;
  }

  UpdateSurfacePropertiesCache();

  UpdateLocalSurfaceIdAndClientSurfaceEmbedder();
}

void ClientRoot::OnLocalSurfaceIdChanged() {
  if (ShouldAssignLocalSurfaceId())
    return;

  HandleBoundsOrScaleFactorChange();
}

void ClientRoot::AllocateLocalSurfaceIdAndNotifyClient() {
  if (!ShouldAssignLocalSurfaceId())
    return;

  // Setting a null LocalSurfaceIdAllocation forces allocating a new one.
  ProxyWindow::GetMayBeNull(window_)->set_local_surface_id_allocation(
      base::nullopt);
  UpdateLocalSurfaceIdAndClientSurfaceEmbedder();
  NotifyClientOfNewBounds();
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

std::unique_ptr<ScopedForceVisible> ClientRoot::ForceWindowVisible() {
  // At this time there is only a need for a single force visible.
  DCHECK(!force_visible_);
  // Use WrapUnique() as constructor is private.
  std::unique_ptr<ScopedForceVisible> force_visible =
      base::WrapUnique(new ScopedForceVisible(this));
  force_visible_ = force_visible.get();
  NotifyClientOfVisibilityChange();
  return force_visible;
}

void ClientRoot::OnWindowTreeHostDisplayIdChanged() {
  if (last_display_id_ != window_->GetHost()->GetDisplayId())
    NotifyClientOfDisplayIdChange();
}

void ClientRoot::UpdateLocalSurfaceIdAndClientSurfaceEmbedder() {
  GenerateLocalSurfaceIdIfNecessary();
  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  if (!proxy_window->local_surface_id_allocation().has_value())
    return;

  const viz::SurfaceId surface_id(
      window_->GetFrameSinkId(),
      proxy_window->local_surface_id_allocation()->local_surface_id());
  const bool surface_id_changed =
      surface_id != client_surface_embedder_->GetSurfaceId();
  client_surface_embedder_->SetSurfaceId(surface_id);

  // This triggers holding events until the frame has been activated. This
  // ensures smooth resizes.
  if (surface_id_changed && ShouldAssignLocalSurfaceId() && window_->GetHost())
    window_->GetHost()->compositor()->OnChildResizing();
}

void ClientRoot::CheckForScaleFactorChange() {
  if (!ShouldAssignLocalSurfaceId() ||
      last_device_scale_factor_ == window_->layer()->device_scale_factor()) {
    return;
  }

  HandleBoundsOrScaleFactorChange();
}

void ClientRoot::HandleBoundsOrScaleFactorChange() {
  if (setting_bounds_from_client_)
    return;

  UpdateLocalSurfaceIdAndClientSurfaceEmbedder();
  NotifyClientOfNewBounds();
}

void ClientRoot::NotifyClientOfNewBounds() {
  last_bounds_ = GetBoundsToSend(window_);
  auto id = ProxyWindow::GetMayBeNull(window_)->local_surface_id_allocation();
  TRACE_EVENT_WITH_FLOW0("ui", "ClientRoot::NotifyClientOfNewBounds",
                         id->local_surface_id().hash(),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  window_tree_->window_tree_client_->OnWindowBoundsChanged(
      window_tree_->TransportIdForWindow(window_), last_bounds_,
      window_->GetProperty(aura::client::kShowStateKey),
      ProxyWindow::GetMayBeNull(window_)->local_surface_id_allocation());
}

void ClientRoot::NotifyClientOfVisibilityChange(base::Optional<bool> visible) {
  const bool new_value = visible.has_value() ? *visible : IsWindowVisible();
  if (last_visible_ == new_value)
    return;

  last_visible_ = new_value;
  if (!window_tree_->property_change_tracker_->IsProcessingChangeForWindow(
          window_, ClientChangeType::kVisibility)) {
    window_tree_->window_tree_client_->OnWindowVisibilityChanged(
        window_tree_->TransportIdForWindow(window_), last_visible_);
  }
}

void ClientRoot::NotifyClientOfDisplayIdChange() {
  last_display_id_ = window_->GetHost()->GetDisplayId();
  window_tree_->window_tree_client_->OnWindowDisplayChanged(
      window_tree_->TransportIdForWindow(window_), last_display_id_);
}

void ClientRoot::OnPositionInRootChanged() {
  DCHECK(!is_top_level_);
  gfx::Rect bounds_in_screen = GetBoundsToSend(window_);
  if (bounds_in_screen.origin() != last_bounds_.origin())
    NotifyClientOfNewBounds();
}

bool ClientRoot::IsWindowVisible() {
  if (force_visible_)
    return true;

  return is_top_level_ ? window_->TargetVisibility() : window_->IsVisible();
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
  if (setting_bounds_from_client_)
    return;

  // Early out when a top level is in middle of moving to a new display.
  // Bounds change will be sent after it is added to the new root window.
  if (is_top_level_ && is_moving_across_displays_)
    return;

  HandleBoundsOrScaleFactorChange();
}

void ClientRoot::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, window_);
  DCHECK(window->GetHost());
  window->GetHost()->AddObserver(this);

  is_moving_across_displays_ = false;
  NotifyClientOfDisplayIdChange();

  // When added to a new root window, the window bounds in screen may have
  // changed even though its bounds in the root window remain the same.
  if (is_top_level_)
    HandleBoundsOrScaleFactorChange();
  else
    CheckForScaleFactorChange();

  NotifyClientOfVisibilityChange();
}

void ClientRoot::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                aura::Window* new_root) {
  DCHECK_EQ(window, window_);
  DCHECK(window->GetHost());
  window->GetHost()->RemoveObserver(this);
  if (new_root) {
    DCHECK(!is_moving_across_displays_);
    is_moving_across_displays_ = true;
  } else {
    NotifyClientOfVisibilityChange(false);
  }
}

void ClientRoot::OnWindowVisibilityChanged(aura::Window* window, bool visible) {
  NotifyClientOfVisibilityChange();
}

void ClientRoot::OnHostResized(aura::WindowTreeHost* host) {
  // This function is also called when the device-scale-factor changes too.
  CheckForScaleFactorChange();
}

void ClientRoot::OnDisplayMetricsChanged(const display::Display& display,
                                         uint32_t changed_metrics) {
  // WindowTreeHost display id should be updated before OnDisplayMetricsChanged.
  // Early out if the changed display is not relevant.
  if (!window_->GetHost() || window_->GetHost()->GetDisplayId() != display.id())
    return;

  // Only handle changes that could change the origin of the ClientRoot.
  if (!(changed_metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION)))
    return;

  // Size or device-scale-factor change is handled in OnHostResized.
  const gfx::Rect new_bounds = GetBoundsToSend(window_);
  if (last_bounds_.size() != new_bounds.size() ||
      last_device_scale_factor_ != ui::GetScaleFactorForNativeView(window_)) {
    return;
  }

  if (last_bounds_ != new_bounds)
    NotifyClientOfNewBounds();
}

void ClientRoot::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  // NOTE: this function is only called if kUseTestConfig is supplied. See
  // call to RegisterFrameSinkId().
  if (window_tree_->client_name().empty())
    return;

  ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window_);
  // OnFirstSurfaceActivation() should only be called after
  // AttachCompositorFrameSink().
  DCHECK(proxy_window->attached_compositor_frame_sink());
  window_tree_->window_service()->OnFirstSurfaceActivation(
      window_tree_->client_name());
}

void ClientRoot::OnFrameTokenChanged(uint32_t frame_token) {
  // TODO: this needs to call through to WindowTreeClient.
  // https://crbug.com/848022.
}

}  // namespace ws
