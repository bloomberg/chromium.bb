// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win_display.h"

#include "ui/display/win/display_info.h"
#include "ui/display/win/screen_win.h"

namespace display {
namespace win {

namespace {

gfx::Display CreateDisplayFromDisplayInfo(const DisplayInfo& display_info) {
  gfx::Display display(display_info.id());
  gfx::Rect dip_screen_bounds(
      display::win::ScreenWin::ScreenToDIPRect(nullptr,
                                               display_info.screen_rect()));
  display.set_bounds(dip_screen_bounds);
  display.set_work_area(
      display::win::ScreenWin::ScreenToDIPRect(
          nullptr, display_info.screen_work_rect()));
  display.SetScaleAndBounds(display_info.device_scale_factor(),
                            display_info.screen_rect());
  display.set_rotation(display_info.rotation());
  return display;
}

} // namespace

ScreenWinDisplay::ScreenWinDisplay() = default;

ScreenWinDisplay::ScreenWinDisplay(const DisplayInfo& display_info)
    : display_(CreateDisplayFromDisplayInfo(display_info)),
      pixel_bounds_(display_info.screen_rect()) {}

}  // namespace win
}  // namespace display
