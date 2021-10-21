// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/snap_controller_impl.h"

#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

SnapControllerImpl::SnapControllerImpl() = default;
SnapControllerImpl::~SnapControllerImpl() = default;

bool SnapControllerImpl::CanSnap(aura::Window* window) {
  return WindowState::Get(window)->CanSnap();
}

void SnapControllerImpl::ShowSnapPreview(aura::Window* window,
                                         chromeos::SnapDirection snap) {
  if (snap == chromeos::SnapDirection::kNone) {
    phantom_window_controller_.reset();
    return;
  }

  if (!phantom_window_controller_ ||
      phantom_window_controller_->window() != window) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(window);
  }
  const SnapViewType snap_type = snap == chromeos::SnapDirection::kPrimary
                                     ? SnapViewType::kPrimary
                                     : SnapViewType::kSecondary;
  gfx::Rect phantom_bounds_in_screen =
      GetDefaultSnappedWindowBoundsInParent(window, snap_type);
  ::wm::ConvertRectToScreen(window->parent(), &phantom_bounds_in_screen);
  phantom_window_controller_->Show(phantom_bounds_in_screen);
}

void SnapControllerImpl::CommitSnap(aura::Window* window,
                                    chromeos::SnapDirection snap) {
  phantom_window_controller_.reset();
  if (snap == chromeos::SnapDirection::kNone)
    return;

  WindowState* window_state = WindowState::Get(window);
  const WMEvent snap_event(snap == chromeos::SnapDirection::kPrimary
                               ? WM_EVENT_SNAP_PRIMARY
                               : WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_event);
}

}  // namespace ash
