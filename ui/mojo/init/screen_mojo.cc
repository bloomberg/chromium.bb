// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/mojo/init/screen_mojo.h"

namespace ui {
namespace mojo {

ScreenMojo::ScreenMojo(const std::vector<gfx::Display>& displays)
    : displays_(displays) {}

ScreenMojo::~ScreenMojo() {}

gfx::Point ScreenMojo::GetCursorScreenPoint() {
  // NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::NativeWindow ScreenMojo::GetWindowUnderCursor() {
  // NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeWindow ScreenMojo::GetWindowAtScreenPoint(const gfx::Point& point) {
  // NOTIMPLEMENTED();
  return nullptr;
}

gfx::Display ScreenMojo::GetPrimaryDisplay() const {
  return displays_[0];
}

gfx::Display ScreenMojo::GetDisplayNearestWindow(gfx::NativeView view) const {
  return GetPrimaryDisplay();
}

gfx::Display ScreenMojo::GetDisplayNearestPoint(const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

int ScreenMojo::GetNumDisplays() const {
  return static_cast<int>(displays_.size());
}

std::vector<gfx::Display> ScreenMojo::GetAllDisplays() const {
  return displays_;
}

gfx::Display ScreenMojo::GetDisplayMatching(const gfx::Rect& match_rect) const {
  int biggest_area = 0;
  gfx::Display result;
  const gfx::Display matching_display;
  for (const gfx::Display& display : displays_) {
    gfx::Rect display_union(match_rect);
    display_union.Union(display.bounds());
    if (!display_union.IsEmpty()) {
      const int area = display_union.width() * display_union.height();
      if (area > biggest_area) {
        biggest_area = area;
        result = display;
      }
    }
  }
  return biggest_area == 0 ? displays_[0] : result;
}

void ScreenMojo::AddObserver(gfx::DisplayObserver* observer) {
  // TODO: add support for display changes.
}

void ScreenMojo::RemoveObserver(gfx::DisplayObserver* observer) {
  // TODO: add support for display changes.
}

}  // namespace mojo
}  // namespace ui
