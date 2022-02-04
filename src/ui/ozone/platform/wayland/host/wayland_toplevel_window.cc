// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

#include <aura-shell-client-protocol.h>
#include <string>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/gtk_shell1.h"
#include "ui/ozone/platform/wayland/host/gtk_surface1.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/extensions/wayland_extension.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#endif

namespace ui {

namespace {
bool decorations_allowed_for_test_ = true;
}

constexpr int kVisibleOnAllWorkspaces = -1;

WaylandToplevelWindow::WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                                             WaylandConnection* connection)
    : WaylandWindow(delegate, connection),
      state_(PlatformWindowState::kNormal) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, AsWmMoveResizeHandler());
}

WaylandToplevelWindow::~WaylandToplevelWindow() = default;

bool WaylandToplevelWindow::CreateShellToplevel() {
  // Certain Wayland compositors (E.g. Mutter) expects wl_surface to have no
  // buffer attached when xdg-surface role is created.
  wl_surface_attach(root_surface()->surface(), nullptr, 0, 0);
  root_surface()->Commit(false);

  ShellObjectFactory factory;
  shell_toplevel_ = factory.CreateShellToplevelWrapper(connection(), this);
  if (!shell_toplevel_) {
    LOG(ERROR) << "Failed to create a ShellToplevel.";
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  shell_toplevel_->SetAppId(window_unique_id_);
#else
  shell_toplevel_->SetAppId(app_id_);
#endif
  shell_toplevel_->SetTitle(window_title_);
  SetSizeConstraints();
  TriggerStateChanges();
  SetUpShellIntegration();
  OnDecorationModeChanged();
  // This could be the proper time to update window mask using
  // NonClientView::GetWindowMask, since |non_client_view| is not created yet
  // during the call to WaylandWindow::Initialize().
  UpdateWindowMask();
  root_surface()->Commit(true);
  return true;
}

void WaylandToplevelWindow::ApplyPendingBounds() {
  if (pending_configures_.empty())
    return;
  DCHECK(shell_toplevel_);

  SetBoundsDip(pending_configures_.back().bounds_dip);
}

void WaylandToplevelWindow::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  DCHECK(shell_toplevel_);

  connection()->event_source()->ResetPointerFlags();
  if (hittest == HTCAPTION)
    shell_toplevel_->SurfaceMove(connection());
  else
    shell_toplevel_->SurfaceResize(connection(), hittest);

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::Show(bool inactive) {
  if (shell_toplevel_)
    return;

  if (!CreateShellToplevel()) {
    Close();
    return;
  }

  UpdateWindowScale(false);

  if (auto* drag_controller = connection()->window_drag_controller())
    drag_controller->OnToplevelWindowCreated(this);

  WaylandWindow::Show(inactive);
}

void WaylandToplevelWindow::Hide() {
  if (!shell_toplevel_)
    return;

  if (child_window()) {
    child_window()->Hide();
    set_child_window(nullptr);
  }
  WaylandWindow::Hide();

  shell_toplevel_.reset();
  connection()->ScheduleFlush();
}

bool WaylandToplevelWindow::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return !!shell_toplevel_ || state_ == PlatformWindowState::kMinimized;
}

void WaylandToplevelWindow::SetTitle(const std::u16string& title) {
  if (window_title_ == title)
    return;

  window_title_ = title;

  if (shell_toplevel_) {
    shell_toplevel_->SetTitle(title);
    connection()->ScheduleFlush();
  }
}

void WaylandToplevelWindow::ToggleFullscreen() {
  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_toplevel_set_fullscreen() is not provided with wl_output, it's up
  // to the compositor to choose which display will be used to map this surface.

  // We must track the previous state to correctly say our state as long as it
  // can be the maximized instead of normal one.
  PlatformWindowState new_state = PlatformWindowState::kUnknown;
  if (state_ == PlatformWindowState::kFullScreen) {
    if (previous_state_ == PlatformWindowState::kMaximized)
      new_state = previous_state_;
    else
      new_state = PlatformWindowState::kNormal;
  } else {
    new_state = PlatformWindowState::kFullScreen;
  }

  SetWindowState(new_state);
}

void WaylandToplevelWindow::Maximize() {
  SetWindowState(PlatformWindowState::kMaximized);
}

void WaylandToplevelWindow::Minimize() {
  SetWindowState(PlatformWindowState::kMinimized);
}

void WaylandToplevelWindow::Restore() {
  DCHECK(shell_toplevel_);

  // Differently from other platforms, under Wayland, unmaximizing the dragged
  // window before starting the drag loop is not needed as it is assumed to be
  // handled at compositor side, just like in xdg_toplevel_surface::move. So
  // skip it if there's a window drag session running.
  auto* drag_controller = connection()->window_drag_controller();
  if (drag_controller &&
      drag_controller->state() != WaylandWindowDragController::State::kIdle) {
    return;
  }

  SetWindowState(PlatformWindowState::kNormal);
}

PlatformWindowState WaylandToplevelWindow::GetPlatformWindowState() const {
  return state_;
}

void WaylandToplevelWindow::Activate() {
  // Activation is supported through optional protocol extensions and hence may
  // or may not work depending on the compositor.  The details depend on the
  // compositor as well; for example, Mutter doesn't bring the window to the top
  // when it requests focus, but instead shows a system popup notification to
  // user.
  //
  // Exo provides activation through aura-shell, Mutter--through gtk-shell.
  //
  // TODO(crbug.com/1175327): add support for xdg-activation.
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_ACTIVATE_SINCE_VERSION) {
    zaura_surface_activate(aura_surface_.get());
  } else if (gtk_surface1_) {
    gtk_surface1_->RequestFocus();
  }
  // This is required as the high level activation might not get a flush for
  // a while. Example: Ash calls OpenURL in Lacros, which activates a window
  // but nothing more happens (until the user moves the mouse over a Lacros
  // window in which case events will start and the activation will come
  // through).
  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!shell_toplevel_)
    return;

  SetSizeConstraints();
}

std::string WaylandToplevelWindow::GetWindowUniqueId() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return window_unique_id_;
#else
  return app_id_;
#endif
}

void WaylandToplevelWindow::SetUseNativeFrame(bool use_native_frame) {
  if (use_native_frame_ == use_native_frame)
    return;
  use_native_frame_ = use_native_frame;
  if (shell_toplevel_)
    OnDecorationModeChanged();

  UpdateWindowMask();
}

bool WaylandToplevelWindow::ShouldUseNativeFrame() const {
  // This depends on availability of xdg-decoration protocol extension.
  // Returns false if there is no xdg-decoration protocol extension provided
  // even if use_native_frame_ is true.
  return use_native_frame_ && const_cast<WaylandToplevelWindow*>(this)
                                  ->connection()
                                  ->xdg_decoration_manager_v1();
}

bool WaylandToplevelWindow::ShouldUpdateWindowShape() const {
  return true;
}

bool WaylandToplevelWindow::CanSetDecorationInsets() const {
  return decorations_allowed_for_test_ &&
         const_cast<WaylandToplevelWindow*>(this)
             ->connection()
             ->SupportsSetWindowGeometry();
}

void WaylandToplevelWindow::SetOpaqueRegion(
    const std::vector<gfx::Rect>* region_px) {
  root_surface()->SetOpaqueRegion(region_px);
}

void WaylandToplevelWindow::SetInputRegion(const gfx::Rect* region_px) {
  if (region_px)
    input_region_px_ = *region_px;
  else
    input_region_px_ = absl::nullopt;
  root_surface()->SetInputRegion(region_px);
}

void WaylandToplevelWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_SET_ASPECT_RATIO_SINCE_VERSION) {
    zaura_surface_set_aspect_ratio(aura_surface_.get(), aspect_ratio.width(),
                                   aspect_ratio.height());
  }
}

absl::optional<std::vector<gfx::Rect>> WaylandToplevelWindow::GetWindowShape()
    const {
  return window_shape_in_dips_;
}

// static
void WaylandToplevelWindow::AllowSettingDecorationInsetsForTest(bool allow) {
  decorations_allowed_for_test_ = allow;
}

void WaylandToplevelWindow::UpdateWindowScale(bool update_bounds) {
  auto old_scale = window_scale();
  WaylandWindow::UpdateWindowScale(update_bounds);
  if (old_scale == window_scale())
    return;

  // Update min/max size in DIP if buffer scale is updated.
  SizeConstraintsChanged();
}

void WaylandToplevelWindow::HandleToplevelConfigure(int32_t width_dip,
                                                    int32_t height_dip,
                                                    bool is_maximized,
                                                    bool is_fullscreen,
                                                    bool is_activated) {
  // Store the old state to propagte state changes if Wayland decides to change
  // the state to something else.
  PlatformWindowState old_state = state_;
  if (state_ == PlatformWindowState::kMinimized && !is_activated) {
    state_ = PlatformWindowState::kMinimized;
  } else if (is_fullscreen) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (is_maximized) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  const bool is_normal = state_ == PlatformWindowState::kNormal;

  bool did_send_delegate_notification = !!requested_window_show_state_count_;
  if (requested_window_show_state_count_)
    requested_window_show_state_count_--;

  const bool did_window_show_state_change = old_state != state_;

  // Update state before notifying delegate.
  const bool did_active_change = is_active_ != is_activated;
  is_active_ = is_activated;

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  //
  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds iff the current state is normal (neither
  // maximized nor fullscreen).
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  if (width_dip > 1 && height_dip > 1) {
    pending_bounds_dip_ = gfx::Rect(0, 0, width_dip, height_dip);
    if (is_normal && frame_insets_px()) {
      pending_bounds_dip_.Inset(
          -gfx::ScaleToRoundedInsets(*frame_insets_px(), 1.f / window_scale()));
      pending_bounds_dip_.set_origin({0, 0});
    }
  } else if (is_normal) {
    pending_bounds_dip_.set_size(
        gfx::ScaleToRoundedSize(GetRestoredBoundsInPixels().IsEmpty()
                                    ? GetBounds().size()
                                    : GetRestoredBoundsInPixels().size(),
                                1.0 / window_scale()));
  }

  // Store the restored bounds if current state differs from the normal state.
  // It can be client or compositor side change from normal to something else.
  // Thus, we must store previous bounds to restore later.
  SetOrResetRestoredBounds();

  if (did_window_show_state_change && !did_send_delegate_notification) {
    previous_state_ = old_state;
    delegate()->OnWindowStateChanged(previous_state_, state_);
  }

  if (did_active_change)
    delegate()->OnActivationChanged(is_active_);

  state_change_in_transit_ = false;
}

void WaylandToplevelWindow::HandleSurfaceConfigure(uint32_t serial) {
  ProcessPendingBoundsDip(serial);
  pending_bounds_dip_ = gfx::Rect();
}

void WaylandToplevelWindow::UpdateVisualSize(const gfx::Size& size_px,
                                             float scale_factor) {
  WaylandWindow::UpdateVisualSize(size_px, scale_factor);

  if (!shell_toplevel_)
    return;

  if (!ProcessVisualSizeUpdate(size_px, scale_factor) &&
      set_geometry_on_next_frame_) {
    auto size_dip = gfx::ScaleToRoundedSize(size_px, 1.f / scale_factor);
    SetWindowGeometry(gfx::Rect(size_dip));
    set_geometry_on_next_frame_ = false;
  }

  // UpdateVisualSize() indicates a frame update, which means we can forward new
  // bounds now. Apply the latest pending_configure.
  ApplyPendingBounds();
}

bool WaylandToplevelWindow::OnInitialize(
    PlatformWindowInitProperties properties) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto token = base::UnguessableToken::Create();
  window_unique_id_ =
      std::string(crosapi::kLacrosAppIdPrefix) + token.ToString();
#else
  app_id_ = properties.wayland_app_id;
#endif
  SetWaylandExtension(this, static_cast<WaylandExtension*>(this));
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));
  SetWorkspaceExtension(this, static_cast<WorkspaceExtension*>(this));
  SetWorkspaceExtensionDelegate(properties.workspace_extension_delegate);
  SetDeskExtension(this, static_cast<DeskExtension*>(this));

  if (!properties.workspace.empty()) {
    int workspace;
    base::StringToInt(properties.workspace, &workspace);
    workspace_ = workspace;
  } else if (properties.visible_on_all_workspaces) {
    workspace_ = kVisibleOnAllWorkspaces;
  }

  SetPinnedModeExtension(this, static_cast<PinnedModeExtension*>(this));
  return true;
}

bool WaylandToplevelWindow::IsActive() const {
  return is_active_;
}

bool WaylandToplevelWindow::IsSurfaceConfigured() {
  return shell_toplevel() ? shell_toplevel()->IsConfigured() : false;
}

void WaylandToplevelWindow::SetWindowGeometry(gfx::Rect bounds_dip) {
  DCHECK(connection()->SupportsSetWindowGeometry());

  if (!shell_toplevel_)
    return;

  if (state_ == PlatformWindowState::kNormal && frame_insets_px()) {
    bounds_dip.Inset(
        gfx::ScaleToRoundedInsets(*frame_insets_px(), 1.f / window_scale()));
  }
  shell_toplevel_->SetWindowGeometry(bounds_dip);
}

void WaylandToplevelWindow::AckConfigure(uint32_t serial) {
  shell_toplevel()->AckConfigure(serial);
}

void WaylandToplevelWindow::UpdateDecorations() {
  if (!state_change_in_transit_)
    set_geometry_on_next_frame_ = true;
}

void WaylandToplevelWindow::OcclusionChanged(void* data,
                                             zaura_surface* surface,
                                             wl_fixed_t occlusion_fraction,
                                             uint32_t occlusion_reason) {}

void WaylandToplevelWindow::LockFrame(void* data, zaura_surface* surface) {
  WaylandToplevelWindow* self = static_cast<WaylandToplevelWindow*>(data);
  DCHECK(self);
  self->OnFrameLockingChanged(true);
}

void WaylandToplevelWindow::UnlockFrame(void* data, zaura_surface* surface) {
  WaylandToplevelWindow* self = static_cast<WaylandToplevelWindow*>(data);
  DCHECK(self);
  self->OnFrameLockingChanged(false);
}

void WaylandToplevelWindow::OcclusionStateChanged(void* data,
                                                  zaura_surface* surface,
                                                  uint32_t mode) {
  WaylandToplevelWindow* self = static_cast<WaylandToplevelWindow*>(data);
  DCHECK(self);
  auto state = PlatformWindowOcclusionState::kUnknown;
  switch (mode) {
    case ZAURA_SURFACE_OCCLUSION_STATE_UNKNOWN:
      state = PlatformWindowOcclusionState::kUnknown;
      break;
    case ZAURA_SURFACE_OCCLUSION_STATE_VISIBLE:
      state = PlatformWindowOcclusionState::kVisible;
      break;
    case ZAURA_SURFACE_OCCLUSION_STATE_OCCLUDED:
      state = PlatformWindowOcclusionState::kOccluded;
      break;
    case ZAURA_SURFACE_OCCLUSION_STATE_HIDDEN:
      state = PlatformWindowOcclusionState::kHidden;
      break;
  }
  self->OnOcclusionStateChanged(state);
}

void WaylandToplevelWindow::DeskChanged(void* data,
                                        zaura_surface* surface,
                                        int state) {
  WaylandToplevelWindow* self = static_cast<WaylandToplevelWindow*>(data);
  DCHECK(self);
  self->OnDeskChanged(state);
}

void WaylandToplevelWindow::StartThrottle(void* data, zaura_surface* surface) {
  WaylandToplevelWindow* self = static_cast<WaylandToplevelWindow*>(data);
  self->delegate()->SetFrameRateThrottleEnabled(true);
}

void WaylandToplevelWindow::EndThrottle(void* data, zaura_surface* surface) {
  WaylandToplevelWindow* self = static_cast<WaylandToplevelWindow*>(data);
  self->delegate()->SetFrameRateThrottleEnabled(false);
}

bool WaylandToplevelWindow::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  DCHECK(connection()->window_drag_controller());
  return connection()->window_drag_controller()->Drag(this, drag_offset);
}

void WaylandToplevelWindow::EndMoveLoop() {
  DCHECK(connection()->window_drag_controller());
  connection()->window_drag_controller()->StopDragging();
}

void WaylandToplevelWindow::StartWindowDraggingSessionIfNeeded() {
  DCHECK(connection()->window_drag_controller());
  connection()->window_drag_controller()->StartDragSession();
}

void WaylandToplevelWindow::SetImmersiveFullscreenStatus(bool status) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_SET_FULLSCREEN_MODE_SINCE_VERSION) {
    auto mode = status ? ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE
                       : ZAURA_SURFACE_FULLSCREEN_MODE_PLAIN;
    zaura_surface_set_fullscreen_mode(aura_surface_.get(), mode);
  } else {
    // TODO(https://crbug.com/1113900): Implement AuraShell support for
    // non-browser windows and replace this if-else clause by a DCHECK.
    NOTIMPLEMENTED_LOG_ONCE()
        << "Implement AuraShell support for non-browser windows.";
  }
}

void WaylandToplevelWindow::ShowSnapPreview(
    WaylandWindowSnapDirection snap_direction,
    bool allow_haptic_feedback) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_INTENT_TO_SNAP_SINCE_VERSION) {
    uint32_t zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_NONE;
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kPrimary:
        zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_LEFT;
        break;
      case WaylandWindowSnapDirection::kSecondary:
        zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_RIGHT;
        break;
      case WaylandWindowSnapDirection::kNone:
        break;
    }
    zaura_surface_intent_to_snap(aura_surface_.get(),
                                 zaura_shell_snap_direction);
    return;
  }

  NOTIMPLEMENTED_LOG_ONCE()
      << "Window snapping isn't available for non-lacros builds.";
}

void WaylandToplevelWindow::CommitSnap(
    WaylandWindowSnapDirection snap_direction) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_UNSET_SNAP_SINCE_VERSION) {
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kPrimary:
        zaura_surface_set_snap_left(aura_surface_.get());
        return;
      case WaylandWindowSnapDirection::kSecondary:
        zaura_surface_set_snap_right(aura_surface_.get());
        return;
      case WaylandWindowSnapDirection::kNone:
        zaura_surface_unset_snap(aura_surface_.get());
        return;
    }
  }

  NOTIMPLEMENTED_LOG_ONCE()
      << "Window snapping isn't available for non-lacros builds.";
}

void WaylandToplevelWindow::SetCanGoBack(bool value) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_SET_CAN_GO_BACK_SINCE_VERSION) {
    if (value)
      zaura_surface_set_can_go_back(aura_surface_.get());
    else
      zaura_surface_unset_can_go_back(aura_surface_.get());
  }
}

void WaylandToplevelWindow::SetPip() {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_SET_PIP_SINCE_VERSION) {
    zaura_surface_set_pip(aura_surface_.get());
  }
}

void WaylandToplevelWindow::Lock(WaylandOrientationLockType lock_type) {
  shell_toplevel_->Lock(lock_type);
}

void WaylandToplevelWindow::Unlock() {
  shell_toplevel_->Unlock();
}

bool WaylandToplevelWindow::SupportsPointerLock() {
  return !!connection()->wayland_zwp_pointer_constraints() &&
         !!connection()->wayland_zwp_relative_pointer_manager();
}
void WaylandToplevelWindow::LockPointer(bool enabled) {
  auto* pointer_constraints = connection()->wayland_zwp_pointer_constraints();
  if (enabled)
    pointer_constraints->LockPointer(root_surface());
  else
    pointer_constraints->UnlockPointer();
}

int WaylandToplevelWindow::GetNumberOfDesks() const {
  auto* zaura_shell =
      const_cast<WaylandToplevelWindow*>(this)->connection()->zaura_shell();
  return zaura_shell ? zaura_shell->GetNumberOfDesks() : 0;
}

int WaylandToplevelWindow::GetActiveDeskIndex() const {
  auto* zaura_shell =
      const_cast<WaylandToplevelWindow*>(this)->connection()->zaura_shell();
  // The index of the active desk is 0 when there is no virtual desk supported.
  return zaura_shell ? zaura_shell->GetActiveDeskIndex() : 0;
}

std::u16string WaylandToplevelWindow::GetDeskName(int index) const {
  auto* zaura_shell =
      const_cast<WaylandToplevelWindow*>(this)->connection()->zaura_shell();
  return zaura_shell ? base::UTF8ToUTF16(zaura_shell->GetDeskName(index))
                     : std::u16string();
}

void WaylandToplevelWindow::SendToDeskAtIndex(int index) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_MOVE_TO_DESK_SINCE_VERSION) {
    zaura_surface_move_to_desk(aura_surface_.get(), index);
  }
}

void WaylandToplevelWindow::Pin(bool trusted) const {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_SET_PIN_SINCE_VERSION) {
    zaura_surface_set_pin(aura_surface_.get(), trusted);
  }
}

void WaylandToplevelWindow::Unpin() const {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_UNSET_PIN_SINCE_VERSION) {
    zaura_surface_unset_pin(aura_surface_.get());
  }
}

std::string WaylandToplevelWindow::GetWorkspace() const {
  return workspace_.has_value() ? base::NumberToString(workspace_.value())
                                : std::string();
}

void WaylandToplevelWindow::SetVisibleOnAllWorkspaces(bool always_visible) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_MOVE_TO_DESK_SINCE_VERSION) {
    SendToDeskAtIndex(always_visible ? kVisibleOnAllWorkspaces
                                     : GetActiveDeskIndex());
  }
}

bool WaylandToplevelWindow::IsVisibleOnAllWorkspaces() const {
  return workspace_ == kVisibleOnAllWorkspaces;
}

void WaylandToplevelWindow::SetWorkspaceExtensionDelegate(
    WorkspaceExtensionDelegate* delegate) {
  workspace_extension_delegate_ = delegate;
}

void WaylandToplevelWindow::TriggerStateChanges() {
  if (!shell_toplevel_)
    return;

  // Call UnSetMaximized only if current state is normal. Otherwise, if the
  // current state is fullscreen and the previous is maximized, calling
  // UnSetMaximized may result in wrong restored window position that clients
  // are not allowed to know about.
  if (state_ == PlatformWindowState::kMinimized) {
    shell_toplevel_->SetMinimized();
  } else if (state_ == PlatformWindowState::kFullScreen) {
    shell_toplevel_->SetFullscreen();
  } else if (previous_state_ == PlatformWindowState::kFullScreen) {
    shell_toplevel_->UnSetFullscreen();
  } else if (state_ == PlatformWindowState::kMaximized) {
    shell_toplevel_->SetMaximized();
  } else if (state_ == PlatformWindowState::kNormal) {
    shell_toplevel_->UnSetMaximized();
  }

  state_change_in_transit_ = (previous_state_ != state_);

  delegate()->OnWindowStateChanged(previous_state_, state_);

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::SetWindowState(PlatformWindowState state) {
  if (state_ != state) {
    previous_state_ = state_;
    state_ = state;

    // Tracks this window show state change request, coming from the Browser.
    requested_window_show_state_count_++;

    TriggerStateChanges();
  }
}

WmMoveResizeHandler* WaylandToplevelWindow::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

void WaylandToplevelWindow::SetSizeConstraints() {
  DCHECK(delegate());

  min_size_ = delegate()->GetMinimumSizeForWindow();
  max_size_ = delegate()->GetMaximumSizeForWindow();

  if (min_size_.has_value()) {
    auto min_size_dip =
        gfx::ScaleToRoundedSize(min_size_.value(), 1.0f / window_scale());
    shell_toplevel_->SetMinSize(min_size_dip.width(), min_size_dip.height());
  }

  if (max_size_.has_value()) {
    auto max_size_dip =
        gfx::ScaleToRoundedSize(max_size_.value(), 1.0f / window_scale());
    shell_toplevel_->SetMaxSize(max_size_dip.width(), max_size_dip.height());
  }

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::SetOrResetRestoredBounds() {
  // The |restored_bounds_| are used when the window gets back to normal
  // state after it went maximized or fullscreen.  So we reset these if the
  // window has just become normal and store the current bounds if it is
  // either going out of normal state or simply changes the state and we don't
  // have any meaningful value stored.
  if (GetPlatformWindowState() == PlatformWindowState::kNormal) {
    SetRestoredBoundsInPixels({});
  } else if (GetRestoredBoundsInPixels().IsEmpty()) {
    SetRestoredBoundsInPixels(GetBounds());
  }
}

void WaylandToplevelWindow::SetUpShellIntegration() {
  // This method should be called after the XDG surface is initialized.
  DCHECK(shell_toplevel_);
  if (connection()->zaura_shell() && !aura_surface_) {
    static constexpr zaura_surface_listener zaura_surface_listener = {
        &OcclusionChanged, &LockFrame,     &UnlockFrame, &OcclusionStateChanged,
        &DeskChanged,      &StartThrottle, &EndThrottle,
    };
    aura_surface_.reset(zaura_shell_get_aura_surface(
        connection()->zaura_shell()->wl_object(), root_surface()->surface()));
    zaura_surface_add_listener(aura_surface_.get(), &zaura_surface_listener,
                               this);
    zaura_surface_set_occlusion_tracking(aura_surface_.get());
    SetImmersiveFullscreenStatus(false);
    SetInitialWorkspace();
  }

  if (connection()->gtk_shell1()) {
    gtk_surface1_ =
        connection()->gtk_shell1()->GetGtkSurface1(root_surface()->surface());
  }
}

void WaylandToplevelWindow::OnDecorationModeChanged() {
  DCHECK(shell_toplevel_);
  if (use_native_frame_) {
    // Set server-side decoration for windows using a native frame,
    // e.g. taskmanager
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kServerSide);
  } else if (aura_surface_ &&
             zaura_surface_get_version(aura_surface_.get()) >=
                 ZAURA_SURFACE_SET_SERVER_START_RESIZE_SINCE_VERSION) {
    // Sets custom-decoration mode for window that supports aura_shell.
    // e.g. lacros-browser.
    zaura_surface_set_server_start_resize(aura_surface_.get());
  } else {
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kClientSide);
  }
}

void WaylandToplevelWindow::OnFrameLockingChanged(bool lock) {
  DCHECK(delegate());
  delegate()->OnSurfaceFrameLockingChanged(lock);
}

void WaylandToplevelWindow::OnOcclusionStateChanged(
    PlatformWindowOcclusionState occlusion_state) {
  delegate()->OnOcclusionStateChanged(occlusion_state);
}

void WaylandToplevelWindow::OnDeskChanged(int state) {
  DCHECK(delegate());
  workspace_ = state;
  if (workspace_extension_delegate_)
    workspace_extension_delegate_->OnWorkspaceChanged();
}

void WaylandToplevelWindow::SetInitialWorkspace() {
  if (!workspace_.has_value())
    return;

  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_SET_INITIAL_WORKSPACE_SINCE_VERSION) {
    zaura_surface_set_initial_workspace(
        aura_surface_.get(), base::NumberToString(workspace_.value()).c_str());
  }
}

void WaylandToplevelWindow::UpdateWindowMask() {
  // TODO(http://crbug.com/1158733): When supporting PlatformWindow::SetShape,
  // update window region with the given |shape|.
  WaylandWindow::UpdateWindowMask();
  gfx::Rect region(visual_size_px());
  root_surface()->SetInputRegion(input_region_px_ ? &*input_region_px_
                                                  : &region);
}

void WaylandToplevelWindow::UpdateWindowShape() {
  // Create |window_shape_in_dips_| using the window mask of
  // PlatformWindowDelegate otherwise resets it.
  SkPath window_mask_in_pixels =
      delegate()->GetWindowMaskForWindowShapeInPixels();
  if (window_mask_in_pixels.isEmpty()) {
    window_shape_in_dips_.reset();
    return;
  }
  SkPath window_mask_in_dips =
      wl::ConvertPathToDIP(window_mask_in_pixels, window_scale());
  window_shape_in_dips_ = wl::CreateRectsFromSkPath(window_mask_in_dips);
}

bool WaylandToplevelWindow::GetTabletMode() {
  return connection()->GetTabletMode();
}

}  // namespace ui
