// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_TEST_SCREEN_H_
#define UI_DISPLAY_TEST_TEST_SCREEN_H_

#include <vector>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace display {
namespace test {

// A dummy implementation of display::Screen that contains a single
// display::Display only. The contained Display can be accessed and modified via
// TestScreen::display().
//
// NOTE: Adding and removing display::DisplayOberver's are no-ops and observers
// will NOT be notified ever.
class TestScreen : public Screen {
 public:
  TestScreen();
  ~TestScreen() override;

  Display* display() { return &display_; }

  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<Display> GetAllDisplays() const override;
  Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  Display GetPrimaryDisplay() const override;
  void AddObserver(DisplayObserver* observer) override;
  void RemoveObserver(DisplayObserver* observer) override;

 private:
  // The only display.
  Display display_;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_TEST_SCREEN_H_
