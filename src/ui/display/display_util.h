// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_UTIL_H_
#define UI_DISPLAY_DISPLAY_UTIL_H_

#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/native_widget_types.h"

namespace display {

class DISPLAY_EXPORT DisplayUtil {
 public:
  static void DisplayToScreenInfo(display::ScreenInfo* screen_info,
                                  const display::Display& display);

  static void GetNativeViewScreenInfo(display::ScreenInfo* screen_info,
                                      gfx::NativeView native_view);

  static void GetDefaultScreenInfo(display::ScreenInfo* screen_info);

  // Compute the orientation type of the display assuming it is a mobile device.
  static display::mojom::ScreenOrientation GetOrientationTypeForMobile(
      const display::Display& display);

  // Compute the orientation type of the display assuming it is a desktop.
  static display::mojom::ScreenOrientation GetOrientationTypeForDesktop(
      const display::Display& display);
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_UTIL_H_
