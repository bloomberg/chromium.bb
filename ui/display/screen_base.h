// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCREEN_BASE_H_
#define UI_DISPLAY_SCREEN_BASE_H_

#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/display_list.h"
#include "ui/display/screen.h"

namespace display {

// Simple screen implementation with a display list.
class DISPLAY_EXPORT ScreenBase : public display::Screen {
 public:
  ScreenBase();
  ~ScreenBase() override;

  display::DisplayList* display_list() { return &display_list_; };

 protected:
  // Invoked when a display changed in some way, including being added.
  // If |is_primary| is true, |changed_display| is the primary display.
  void ProcessDisplayChanged(const display::Display& changed_display,
                             bool is_primary);

 private:
  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  int GetNumDisplays() const override;
  std::vector<display::Display> GetAllDisplays() const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  display::DisplayList display_list_;

  DISALLOW_COPY_AND_ASSIGN(ScreenBase);
};

}  // namespace display

#endif  // UI_DISPLAY_SCREEN_BASE_H_
