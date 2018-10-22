// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "chromeos/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/controls/image_view.h"

namespace ash {

VirtualKeyboardTray::VirtualKeyboardTray(Shelf* shelf)
    : TrayBackgroundView(shelf), icon_(new views::ImageView), shelf_(shelf) {
  SetInkDropMode(InkDropMode::ON);

  UpdateIcon();
  tray_container()->AddChildView(icon_);

  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    Shell::Get()->accessibility_controller()->AddObserver(this);
    Shell::Get()->AddShellObserver(this);
  }
  // Try observing keyboard controller, in case it is already constructed.
  ObserveKeyboardController();
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // Try unobserving keyboard controller, in case it still exists.
  UnobserveKeyboardController();
  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    Shell::Get()->RemoveShellObserver(this);
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
  }
}

base::string16 VirtualKeyboardTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_VIRTUAL_KEYBOARD_TRAY_ACCESSIBLE_NAME);
}

void VirtualKeyboardTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void VirtualKeyboardTray::ClickedOutsideBubble() {}

bool VirtualKeyboardTray::PerformAction(const ui::Event& event) {
  UserMetricsRecorder::RecordUserClickOnTray(
      LoginMetricsRecorder::TrayClickTarget::kVirtualKeyboardTray);

  auto* keyboard_controller = keyboard::KeyboardController::Get();
  // Keyboard may not always be enabled. https://crbug.com/749989
  if (keyboard_controller->enabled()) {
    keyboard_controller->ShowKeyboardInDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            shelf_->GetWindow()));
  }
  // Normally, active status is set when virtual keyboard is shown/hidden,
  // however, showing virtual keyboard happens asynchronously and, especially
  // the first time, takes some time. We need to set active status here to
  // prevent bad things happening if user clicked the button before keyboard is
  // shown.
  SetIsActive(true);
  return true;
}

void VirtualKeyboardTray::OnAccessibilityStatusChanged() {
  bool new_enabled =
      Shell::Get()->accessibility_controller()->IsVirtualKeyboardEnabled();
  SetVisible(new_enabled);
  if (new_enabled) {
    // Observe keyboard controller to detect when the virtual keyboard is
    // shown/hidden.
    ObserveKeyboardController();
  } else {
    // Try unobserving keyboard controller, in case it is not yet destroyed.
    UnobserveKeyboardController();
  }
}

void VirtualKeyboardTray::OnKeyboardVisibilityStateChanged(
    const bool is_visible) {
  SetIsActive(is_visible);
}

void VirtualKeyboardTray::OnKeyboardControllerCreated() {
  ObserveKeyboardController();
}

void VirtualKeyboardTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateIcon();
}

void VirtualKeyboardTray::ObserveKeyboardController() {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->enabled() && !keyboard_controller->HasObserver(this))
    keyboard_controller->AddObserver(this);
}

void VirtualKeyboardTray::UnobserveKeyboardController() {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->enabled())
    keyboard_controller->RemoveObserver(this);
}

void VirtualKeyboardTray::UpdateIcon() {
  const gfx::VectorIcon& icon = chromeos::switches::ShouldUseShelfNewUi()
                                    ? kShelfKeyboardNewuiIcon
                                    : kShelfKeyboardIcon;
  gfx::ImageSkia image = gfx::CreateVectorIcon(
      icon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
  icon_->SetImage(image);
  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
}

}  // namespace ash
