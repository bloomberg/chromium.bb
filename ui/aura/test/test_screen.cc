// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_screen.h"

#include "base/logging.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"

namespace aura {

TestScreen::TestScreen(aura::RootWindow* root_window)
    : root_window_(root_window) {
}

TestScreen::~TestScreen() {
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return root_window_->last_mouse_location();
}

gfx::NativeWindow TestScreen::GetWindowAtCursorScreenPoint() {
  const gfx::Point point = gfx::Screen::GetCursorScreenPoint();
  return root_window_->GetTopWindowContainingPoint(point);
}

int TestScreen::GetNumDisplays() {
  return 1;
}

gfx::Display TestScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetMonitor();
}

gfx::Display TestScreen::GetDisplayNearestPoint(const gfx::Point& point) const {
  return GetMonitor();
}

gfx::Display TestScreen::GetPrimaryDisplay() const {
  return GetMonitor();
}

gfx::Display TestScreen::GetMonitor() const {
  return gfx::Display(0, root_window_->bounds());
}

}  // namespace aura
