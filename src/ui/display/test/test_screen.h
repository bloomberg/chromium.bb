// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_TEST_SCREEN_H_
#define UI_DISPLAY_TEST_TEST_SCREEN_H_

#include <vector>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

namespace display {
namespace test {

// A dummy implementation of Screen that contains a single
// Display only. The contained Display can be accessed and modified via
// TestScreen::display().
//
// NOTE: Adding and removing DisplayOberver's are no-ops and observers
// will NOT be notified ever.
class TestScreen : public ScreenBase {
 public:
  TestScreen();
  ~TestScreen() override;

  // Sets the fake cursor location for the TestScreen.
  void set_cursor_screen_point(const gfx::Point& point);

  // Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;

 private:
  gfx::Point cursor_screen_point_;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_TEST_SCREEN_H_
