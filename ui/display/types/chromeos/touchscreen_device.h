// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_CHROMEOS_TOUCHSCREEN_DEVICE_H_
#define UI_DISPLAY_TYPES_CHROMEOS_TOUCHSCREEN_DEVICE_H_

#include "ui/display/types/display_types_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

// Represents a Touchscreen device state.
struct DISPLAY_TYPES_EXPORT TouchscreenDevice {
  static const int kInvalidId;

  TouchscreenDevice(int id, const gfx::Size& size);

  // ID of the touch screen. This ID must uniquely identify the touch screen.
  int id;

  // Size of the touch screen area.
  gfx::Size size;
};

}  // namespace ui

#endif  // UI_DISPLAY_TYPES_CHROMEOS_TOUCHSCREEN_DEVICE_H_
