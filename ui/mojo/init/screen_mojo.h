// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MOJO_INIT_SCREEN_MOJO_H_
#define UI_MOJO_INIT_SCREEN_MOJO_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ui {
namespace mojo {

class ScreenMojo : public gfx::Screen {
 public:
  explicit ScreenMojo(const std::vector<gfx::Display>& displays);
  ~ScreenMojo() override;

  // gfx::Screen:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::Display GetPrimaryDisplay() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

 private:
  const std::vector<gfx::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMojo);
};

}  // namespace mojo
}  // namespace ui

#endif  // UI_MOJO_INIT_SCREEN_MOJO_H_
