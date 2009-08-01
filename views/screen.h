// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_SCREEN_H_
#define VIEWS_SCREEN_H_

#include "base/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"

namespace views {

// A utility class for getting various info about screen size, monitors,
// cursor position, etc.
// TODO(erikkay) add more of those methods here
class Screen {
 public:
  static gfx::Point GetCursorScreenPoint();

  // Returns the work area of the monitor nearest the specified window.
  static gfx::Rect GetMonitorWorkAreaNearestWindow(gfx::NativeWindow window);
};

}  // namespace views

#endif  // VIEWS_SCREEN_H_
