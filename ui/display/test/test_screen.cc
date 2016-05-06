// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ui/display/test/test_screen.h"

namespace display {
namespace test {

TestScreen::TestScreen() {
}

TestScreen::~TestScreen() {
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return gfx::Point();
}

bool TestScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return false;
}

gfx::NativeWindow TestScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return nullptr;
}

int TestScreen::GetNumDisplays() const {
  return 1;
}

std::vector<Display> TestScreen::GetAllDisplays() const {
  return std::vector<Display>(1, display_);
}

Display TestScreen::GetDisplayNearestWindow(gfx::NativeView view) const {
  return display_;
}

Display TestScreen::GetDisplayNearestPoint(const gfx::Point& point) const {
  return display_;
}

Display TestScreen::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return display_;
}

Display TestScreen::GetPrimaryDisplay() const {
  return display_;
}

void TestScreen::AddObserver(DisplayObserver* observer) {
}

void TestScreen::RemoveObserver(DisplayObserver* observer) {
}

}  // namespace test
}  // namespace display
