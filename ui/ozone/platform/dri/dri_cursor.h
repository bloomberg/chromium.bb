// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_CURSOR_H_
#define UI_OZONE_PLATFORM_DRI_DRI_CURSOR_H_

#include "base/memory/ref_counted.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class PointF;
class Vector2dF;
}

namespace ui {

class BitmapCursorOzone;
class BitmapCursorFactoryOzone;
class DriWindowManager;
class HardwareCursorDelegate;

class DriCursor : public CursorDelegateEvdev {
 public:
  explicit DriCursor(HardwareCursorDelegate* hardware,
                     DriWindowManager* window_manager);
  virtual ~DriCursor();

  void SetCursor(gfx::AcceleratedWidget widget, PlatformCursor platform_cursor);
  void ShowCursor();
  void HideCursor();
  gfx::AcceleratedWidget GetCursorWindow();

  // CursorDelegateEvdev:
  virtual void MoveCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location) OVERRIDE;
  virtual void MoveCursor(const gfx::Vector2dF& delta) OVERRIDE;
  virtual bool IsCursorVisible() OVERRIDE;
  virtual gfx::PointF location() OVERRIDE;

 private:
  // The location of the bitmap (the cursor location is the hotspot location).
  gfx::Point bitmap_location();

  // The DRI implementation for setting the hardware cursor.
  HardwareCursorDelegate* hardware_;

  DriWindowManager* window_manager_;  // Not owned.

  // The current cursor bitmap.
  scoped_refptr<BitmapCursorOzone> cursor_;

  // The window under the cursor.
  gfx::AcceleratedWidget cursor_window_;

  // The location of the cursor within the window.
  gfx::PointF cursor_location_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_CURSOR_H_
