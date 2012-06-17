// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_H_
#define UI_GFX_SCREEN_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {
class Rect;
class ScreenImpl;

// A utility class for getting various info about screen size, displays,
// cursor position, etc.
class UI_EXPORT Screen {
 public:
#if defined(USE_AURA)
  // Sets the instance to use. This takes owernship of |screen|, deleting the
  // old instance. This is used on aura to avoid circular dependencies between
  // ui and aura.
  static void SetInstance(ScreenImpl* screen);
#endif

  // Returns true if DIP is enabled.
  static bool IsDIPEnabled();

  // Returns the current absolute position of the mouse pointer.
  static gfx::Point GetCursorScreenPoint();

  // Returns the window under the cursor.
  static gfx::NativeWindow GetWindowAtCursorScreenPoint();

  // Returns the number of displays.
  // Mirrored displays are excluded; this method is intended to return the
  // number of distinct, usable displays.
  static int GetNumDisplays();

  // Returns the display nearest the specified window.
  static gfx::Display GetDisplayNearestWindow(gfx::NativeView view);

  // Returns the the display nearest the specified point.
  static gfx::Display GetDisplayNearestPoint(const gfx::Point& point);

  // Returns the bounds of the work area of the primary display.
  static gfx::Display GetPrimaryDisplay();

  // Returns the display that most closely intersects the provided bounds.
  static gfx::Display GetDisplayMatching(const gfx::Rect& match_rect);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Screen);
};

}  // namespace gfx

#endif  // UI_GFX_SCREEN_H_
