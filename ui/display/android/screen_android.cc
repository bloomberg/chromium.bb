// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace display {

class ScreenAndroid : public ScreenBase {
 public:
  ScreenAndroid() {
    gfx::DeviceDisplayInfo device_info;
    const float device_scale_factor = device_info.GetDIPScale();
    // Note: GetPhysicalDisplayWidth/Height() does not subtract window
    // decorations etc. Use it instead of GetDisplayWidth/Height() when
    // available.
    const gfx::Rect bounds_in_pixels =
        gfx::Rect(device_info.GetPhysicalDisplayWidth()
                      ? device_info.GetPhysicalDisplayWidth()
                      : device_info.GetDisplayWidth(),
                  device_info.GetPhysicalDisplayHeight()
                      ? device_info.GetPhysicalDisplayHeight()
                      : device_info.GetDisplayHeight());
    const gfx::Rect bounds_in_dip = gfx::Rect(gfx::ScaleToCeiledSize(
        bounds_in_pixels.size(), 1.0f / device_scale_factor));
    Display display(0, bounds_in_dip);
    if (!Display::HasForceDeviceScaleFactor())
      display.set_device_scale_factor(device_scale_factor);
    display.SetRotationAsDegree(device_info.GetRotationDegrees());
    display.set_color_depth(device_info.GetBitsPerPixel());
    display.set_depth_per_component(device_info.GetBitsPerComponent());
    display.set_is_monochrome(device_info.GetBitsPerComponent() == 0);
    ProcessDisplayChanged(display, true /* is_primary */);
  }

  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }

  bool IsWindowUnderCursor(gfx::NativeWindow window) override {
    NOTIMPLEMENTED();
    return false;
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return NULL;
  }

  Display GetDisplayNearestWindow(gfx::NativeView view) const override {
    return GetPrimaryDisplay();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAndroid);
};

Screen* CreateNativeScreen() {
  return new ScreenAndroid;
}

}  // namespace display
