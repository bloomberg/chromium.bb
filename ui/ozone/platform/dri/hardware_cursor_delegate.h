// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_HARDWARE_CURSOR_DELEGATE_H_
#define UI_OZONE_PLATFORM_DRI_HARDWARE_CURSOR_DELEGATE_H_

#include "ui/gfx/native_widget_types.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class HardwareCursorDelegate {
 public:
  // Update the HW cursor bitmap & move to specified location. If
  // the bitmap is empty, the cursor is hidden.
  virtual void SetHardwareCursor(gfx::AcceleratedWidget widget,
                                 const SkBitmap& bitmap,
                                 const gfx::Point& location) = 0;

  // Move the HW cursor to the specified location.
  virtual void MoveHardwareCursor(gfx::AcceleratedWidget widget,
                                  const gfx::Point& location) = 0;

 protected:
  virtual ~HardwareCursorDelegate() {}
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_HARDWARE_CURSOR_DELEGATE_H_
