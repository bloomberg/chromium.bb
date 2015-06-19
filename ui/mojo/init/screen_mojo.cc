// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/mojo/init/screen_mojo.h"

namespace ui {
namespace mojo {

ScreenMojo::ScreenMojo(const gfx::Size& screen_size_in_pixels,
                       float device_pixel_ratio)
    : screen_size_in_pixels_(screen_size_in_pixels),
      device_pixel_ratio_(device_pixel_ratio) {
  static int64 synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(device_pixel_ratio,
                             gfx::Rect(screen_size_in_pixels));
}

gfx::Point ScreenMojo::GetCursorScreenPoint() {
  return gfx::Point();
}

gfx::NativeWindow ScreenMojo::GetWindowUnderCursor() {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeWindow ScreenMojo::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::Display ScreenMojo::GetPrimaryDisplay() const {
  return display_;
}

gfx::Display ScreenMojo::GetDisplayNearestWindow(gfx::NativeView view) const {
  return GetPrimaryDisplay();
}

gfx::Display ScreenMojo::GetDisplayNearestPoint(const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

int ScreenMojo::GetNumDisplays() const {
  return 1;
}

std::vector<gfx::Display> ScreenMojo::GetAllDisplays() const {
  return std::vector<gfx::Display>(1, GetPrimaryDisplay());
}

gfx::Display ScreenMojo::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

void ScreenMojo::AddObserver(gfx::DisplayObserver* observer) {
  // TODO: add support for display changes.
}

void ScreenMojo::RemoveObserver(gfx::DisplayObserver* observer) {
  // TODO: add support for display changes.
}

}  // namespace mojo
}  // namespace ui
