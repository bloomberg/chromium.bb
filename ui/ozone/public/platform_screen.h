// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_SCREEN_H_
#define UI_OZONE_PUBLIC_PLATFORM_SCREEN_H_

#include "ui/gfx/native_widget_types.h"

namespace display {
class Display;
class DisplayObserver;
}  // namespace display

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {

class PlatformScreen {
 public:
  PlatformScreen() = default;
  virtual ~PlatformScreen() = default;

  virtual const std::vector<display::Display>& GetAllDisplays() const = 0;

  virtual display::Display GetPrimaryDisplay() const = 0;

  // Returns Desktop objects that the |widget| belongs to.
  virtual display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const = 0;

  // Returns cursor position in DIPs relative to the desktop.
  virtual gfx::Point GetCursorScreenPoint() const = 0;

  virtual gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const = 0;

  // Returns the display nearest the specified point. |point| must be in DIPs.
  virtual display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const = 0;

  // Returns the display that most closely intersects the provided rect.
  virtual display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const = 0;

  // Adds/Removes display observers.
  virtual void AddObserver(display::DisplayObserver* observer) = 0;
  virtual void RemoveObserver(display::DisplayObserver* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformScreen);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_SCREEN_H_