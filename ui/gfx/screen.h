// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_H_
#define UI_GFX_SCREEN_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace gfx {

// A utility class for getting various info about screen size, monitors,
// cursor position, etc.
// TODO(erikkay) add more of those methods here
class UI_EXPORT Screen {
 public:
  virtual ~Screen() {}

#if defined(USE_AURA)
  // Sets the instance to use. This takes owernship of |screen|, deleting the
  // old instance. This is used on aura to avoid circular dependencies between
  // ui and aura.
  static void SetInstance(Screen* screen);
#endif

  // Returns the current absolute position of the mouse pointer.
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

  // Returns the bounds of the work area of the primary monitor.
  static gfx::Rect GetPrimaryMonitorWorkArea();

  // Returns the bounds of the primary monitor.
  static gfx::Rect GetPrimaryMonitorBounds();

  // Returns the bounds of the work area of the monitor that most closely
  // intersects the provided bounds.
  static gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect);

  // Returns the window under the cursor.
  static gfx::NativeWindow GetWindowAtCursorScreenPoint();

  // Returns the dimensions of the primary monitor in pixels.
  static gfx::Size GetPrimaryMonitorSize();

  // Returns the number of monitors.
  // Mirrored displays are excluded; this method is intended to return the
  // number of distinct, usable displays.
  static int GetNumMonitors();

 protected:
  virtual gfx::Point GetCursorScreenPointImpl() = 0;
  virtual gfx::Rect GetMonitorWorkAreaNearestWindowImpl(
      gfx::NativeView view) = 0;
  virtual gfx::Rect GetMonitorAreaNearestWindowImpl(
      gfx::NativeView view) = 0;
  virtual gfx::Rect GetMonitorWorkAreaNearestPointImpl(
      const gfx::Point& point) = 0;
  virtual gfx::Rect GetMonitorAreaNearestPointImpl(const gfx::Point& point) = 0;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPointImpl() = 0;
  virtual gfx::Size GetPrimaryMonitorSizeImpl() = 0;
  virtual int GetNumMonitorsImpl() = 0;

private:
#if defined(USE_AURA)
  // The singleton screen instance. Only used on aura.
  static Screen* instance_;
#endif
};

}  // namespace gfx

#endif  // VIEWS_SCREEN_H_
