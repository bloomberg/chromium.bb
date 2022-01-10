// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_delegate.h"

#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {
const int kNullWindowState = -1;
}  // namespace

namespace ash {
namespace full_restore {

ArcGhostWindowDelegate::ArcGhostWindowDelegate(
    exo::ClientControlledShellSurface* shell_surface,
    ArcWindowHandler* handler,
    int window_id,
    int64_t display_id,
    const gfx::Rect& bounds)
    : window_id_(window_id),
      bounds_(gfx::Rect(bounds)),
      pending_close_(false),
      window_state_(chromeos::WindowStateType::kDefault),
      shell_surface_(shell_surface),
      arc_handler_(handler) {
  DCHECK(shell_surface);
  DCHECK(handler);

  observation_.Observe(handler);
  SetDisplayId(display_id);
}
ArcGhostWindowDelegate::~ArcGhostWindowDelegate() = default;

void ArcGhostWindowDelegate::OnGeometryChanged(const gfx::Rect& geometry) {}

void ArcGhostWindowDelegate::OnStateChanged(
    chromeos::WindowStateType old_state_type,
    chromeos::WindowStateType new_state) {
  if (old_state_type == new_state)
    return;

  auto* window_state =
      ash::WindowState::Get(shell_surface_->GetWidget()->GetNativeWindow());

  if (!window_state || !shell_surface_->host_window()->GetRootWindow())
    return;

  display::Display display;
  const display::Screen* screen = display::Screen::GetScreen();
  auto display_existed = screen->GetDisplayWithDisplayId(display_id_, &display);
  DCHECK(display_existed);

  switch (new_state) {
    case chromeos::WindowStateType::kNormal:
    case chromeos::WindowStateType::kDefault:
      // Reset geometry for previous bounds.
      shell_surface_->SetBounds(display_id_, bounds_);
      shell_surface_->SetRestored();
      break;
    case chromeos::WindowStateType::kMinimized:
      shell_surface_->SetMinimized();
      break;
    case chromeos::WindowStateType::kMaximized:
      shell_surface_->SetMaximized();
      // Update geometry for showing overlay as maximized bounds.
      shell_surface_->SetBounds(display_id_, display.bounds());
      break;
    case chromeos::WindowStateType::kFullscreen:
      // TODO(sstan): Adjust bounds like maximized state.
      shell_surface_->SetFullscreen(true);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
  shell_surface_->OnSurfaceCommit();
  window_state_ = new_state;
}

void ArcGhostWindowDelegate::OnBoundsChanged(
    chromeos::WindowStateType current_state,
    chromeos::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds_in_screen,
    bool is_resize,
    int bounds_change) {
  auto* window_state =
      ash::WindowState::Get(shell_surface_->GetWidget()->GetNativeWindow());

  if (!window_state || !shell_surface_->host_window()->GetRootWindow())
    return;

  display::Display target_display;
  const display::Screen* screen = display::Screen::GetScreen();

  if (!screen->GetDisplayWithDisplayId(display_id, &target_display))
    return;

  if (display_id_ != display_id) {
    if (!SetDisplayId(display_id))
      return;
  }

  // Don't change the bounds in maximize/fullscreen/pinned state.
  if (window_state->IsMaximizedOrFullscreenOrPinned() &&
      requested_state == window_state->GetStateType()) {
    return;
  }

  shell_surface_->SetBounds(display_id, bounds_in_screen);

  if (requested_state != window_state->GetStateType()) {
    DCHECK(requested_state == chromeos::WindowStateType::kPrimarySnapped ||
           requested_state == chromeos::WindowStateType::kSecondarySnapped);

    if (requested_state == chromeos::WindowStateType::kPrimarySnapped)
      shell_surface_->SetSnappedToPrimary();
    else
      shell_surface_->SetSnappedToSecondary();
    // TODO(sstan): Currently the snap state will be ignored. Sync it to ARC.
  }
  shell_surface_->OnSurfaceCommit();
  bounds_ = gfx::Rect(bounds_in_screen);
  UpdateWindowInfoToArc();
}

void ArcGhostWindowDelegate::OnDragStarted(int component) {}

void ArcGhostWindowDelegate::OnDragFinished(int x, int y, bool canceled) {}

void ArcGhostWindowDelegate::OnZoomLevelChanged(exo::ZoomChange zoom_change) {}

// ArcWindowHandler::Observer
void ArcGhostWindowDelegate::OnAppInstanceConnected() {
  // Update window info to ARC when app instance connected, since the previous
  // window info may not be delivered.
  UpdateWindowInfoToArc();
}

void ArcGhostWindowDelegate::OnWindowCloseRequested(int window_id) {
  if (window_id != window_id_)
    return;
  pending_close_ = true;
  UpdateWindowInfoToArc();
}

bool ArcGhostWindowDelegate::SetDisplayId(int64_t display_id) {
  absl::optional<double> scale_factor = GetDisplayScaleFactor(display_id);
  if (!scale_factor.has_value()) {
    LOG(ERROR) << "Invalid display id for ARC Ghost Window";
    scale_factor_ = 1.;
    return false;
  }
  scale_factor_ = scale_factor.value();
  display_id_ = display_id;
  return true;
}

void ArcGhostWindowDelegate::UpdateWindowInfoToArc() {
  arc_handler_->OnWindowInfoUpdated(
      window_id_, pending_close_ ? kNullWindowState : (int)window_state_,
      display_id_, gfx::ScaleToRoundedRect(bounds_, scale_factor_));
}

}  // namespace full_restore
}  // namespace ash
