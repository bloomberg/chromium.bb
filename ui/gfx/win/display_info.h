// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_DISPLAY_INFO_H_
#define UI_GFX_WIN_DISPLAY_INFO_H_

#include <windows.h>
#include <stdint.h>

#include "ui/gfx/display.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {
namespace win {

// Gathers the parameters necessary to create a gfx::win::ScreenWinDisplay.
struct GFX_EXPORT DisplayInfo final {
  DisplayInfo(const MONITORINFOEX& monitor_info, float device_scale_factor);
  DisplayInfo(const MONITORINFOEX& monitor_info,
              float device_scale_factor,
              gfx::Display::Rotation rotation);

  static int64_t DeviceIdFromDeviceName(const wchar_t* device_name);

  int64_t id;
  gfx::Display::Rotation rotation;
  gfx::Rect screen_rect;
  gfx::Rect screen_work_rect;
  float device_scale_factor;
};

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_WIN_DISPLAY_INFO_H_
