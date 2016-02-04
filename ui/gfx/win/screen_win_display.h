// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_SCREEN_WIN_DISPLAY_H_
#define UI_GFX_WIN_SCREEN_WIN_DISPLAY_H_

#include <windows.h>

#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
namespace win {

struct DisplayInfo;

// A display used by gfx::ScreenWin.
// It holds a display and additional parameters used for DPI calculations.
struct ScreenWinDisplay final {
  ScreenWinDisplay();
  explicit ScreenWinDisplay(const DisplayInfo& display_info);

  gfx::Display display;
  gfx::Rect pixel_bounds;
};

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_WIN_SCREEN_WIN_DISPLAY_H_
