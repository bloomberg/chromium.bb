// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_H_
#define UI_GFX_SCREEN_H_
#pragma once

#include "ui/base/ui_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/point.h"

namespace gfx {
class Rect;
class ScreenImpl;

// A utility class for getting various info about screen size, monitors,
// cursor position, etc.
class UI_EXPORT Screen {
 public:
  virtual ~Screen() {}

#if defined(USE_AURA)
  // Sets the instance to use. This takes owernship of |screen|, deleting the
  // old instance. This is used on aura to avoid circular dependencies between
  // ui and aura.
  static void SetInstance(ScreenImpl* screen);
#endif

  // True if DIP is enabled.
  static bool IsDIPEnabled();

  // Returns the current absolute position of the mouse pointer.
  static gfx::Point GetCursorScreenPoint();

  // Returns the window under the cursor.
  static gfx::NativeWindow GetWindowAtCursorScreenPoint();

  // Returns the number of monitors.
  // Mirrored displays are excluded; this method is intended to return the
  // number of distinct, usable displays.
  static int GetNumMonitors();

  // Returns the monitor nearest the specified window.
  static gfx::Monitor GetMonitorNearestWindow(gfx::NativeView view);

  // Returns the the monitor nearest the specified point.
  static gfx::Monitor GetMonitorNearestPoint(const gfx::Point& point);

  // Returns the bounds of the work area of the primary monitor.
  static gfx::Monitor GetPrimaryMonitor();

  // Returns the monitor that most closely intersects the provided bounds.
  static gfx::Monitor GetMonitorMatching(const gfx::Rect& match_rect);
};

}  // namespace gfx

#endif  // VIEWS_SCREEN_H_
