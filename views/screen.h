// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_SCREEN_H_
#define VIEWS_SCREEN_H_
#pragma once

#include "gfx/native_widget_types.h"
#include "gfx/point.h"
#include "gfx/rect.h"

namespace views {

// A utility class for getting various info about screen size, monitors,
// cursor position, etc.
// TODO(erikkay) add more of those methods here
class Screen {
 public:
  static gfx::Point GetCursorScreenPoint();

  // Returns the work area of the monitor nearest the specified window.
  static gfx::Rect GetMonitorWorkAreaNearestWindow(gfx::NativeView view);

  // Returns the bounds of the monitor nearest the specified window.
  static gfx::Rect GetMonitorAreaNearestWindow(gfx::NativeView view);

  // Returns the work area of the monitor nearest the specified point.
  static gfx::Rect GetMonitorWorkAreaNearestPoint(const gfx::Point& point);

  // Returns the monitor area (not the work area, but the complete bounds) of
  // the monitor nearest the specified point.
  static gfx::Rect GetMonitorAreaNearestPoint(const gfx::Point& point);

  // Returns the window under the cursor.
  static gfx::NativeWindow GetWindowAtCursorScreenPoint();
};

}  // namespace views

#endif  // VIEWS_SCREEN_H_
