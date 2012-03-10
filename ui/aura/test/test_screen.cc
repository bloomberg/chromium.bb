// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_screen.h"

#include "base/logging.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {

TestScreen::TestScreen(aura::RootWindow* root_window)
    : root_window_(root_window) {
}

TestScreen::~TestScreen() {
}

gfx::Point TestScreen::GetCursorScreenPointImpl() {
  return root_window_->last_mouse_location();
}

gfx::Rect TestScreen::GetMonitorWorkAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  return GetBounds();
}

gfx::Rect TestScreen::GetMonitorAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  return GetBounds();
}

gfx::Rect TestScreen::GetMonitorWorkAreaNearestPointImpl(
    const gfx::Point& point) {
  return GetBounds();
}

gfx::Rect TestScreen::GetMonitorAreaNearestPointImpl(const gfx::Point& point) {
  return GetBounds();
}

gfx::NativeWindow TestScreen::GetWindowAtCursorScreenPointImpl() {
  const gfx::Point point = GetCursorScreenPoint();
  return root_window_->GetTopWindowContainingPoint(point);
}

gfx::Rect TestScreen::GetBounds() {
  return gfx::Rect(root_window_->bounds().size());
}

gfx::Size TestScreen::GetPrimaryMonitorSizeImpl() {
  return GetBounds().size();
}

int TestScreen::GetNumMonitorsImpl() {
  return 1;
}

}  // namespace aura
