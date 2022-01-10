// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/snap_controller_lacros.h"

#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace {

ui::WaylandWindowSnapDirection ToWaylandWindowSnapDirection(
    chromeos::SnapDirection snap) {
  switch (snap) {
    case chromeos::SnapDirection::kNone:
      return ui::WaylandWindowSnapDirection::kNone;
    case chromeos::SnapDirection::kPrimary:
      return ui::WaylandWindowSnapDirection::kPrimary;
    case chromeos::SnapDirection::kSecondary:
      return ui::WaylandWindowSnapDirection::kSecondary;
  }
}

ui::WaylandExtension* WaylandExtensionForAuraWindow(aura::Window* window) {
  // Lacros is based on Ozone/Wayland, which uses ui::PlatformWindow and
  // views::DesktopWindowTreeHostLinux.
  auto* dwth_linux = views::DesktopWindowTreeHostLinux::From(window->GetHost());
  return dwth_linux->GetWaylandExtension();
}

}  // namespace

SnapControllerLacros::SnapControllerLacros() = default;
SnapControllerLacros::~SnapControllerLacros() = default;

bool SnapControllerLacros::CanSnap(aura::Window* window) {
  // TODO(https://crbug.com/1141701): Implement this method similarly to
  // ash::WindowState::CanSnap().
  return true;
}
void SnapControllerLacros::ShowSnapPreview(aura::Window* window,
                                           chromeos::SnapDirection snap,
                                           bool allow_haptic_feedback) {
  auto* wayland_extension = WaylandExtensionForAuraWindow(window);
  wayland_extension->ShowSnapPreview(ToWaylandWindowSnapDirection(snap),
                                     allow_haptic_feedback);
}

void SnapControllerLacros::CommitSnap(aura::Window* window,
                                      chromeos::SnapDirection snap) {
  auto* wayland_extension = WaylandExtensionForAuraWindow(window);
  wayland_extension->CommitSnap(ToWaylandWindowSnapDirection(snap));
}
