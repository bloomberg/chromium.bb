// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_IMPL_H_
#define UI_GFX_SCREEN_IMPL_H_
#pragma once

#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {

// A class that provides |gfx::Screen|'s implementation on aura.
class UI_EXPORT ScreenImpl {
 public:
  virtual ~ScreenImpl() {}

  virtual gfx::Point GetCursorScreenPoint() = 0;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPoint() = 0;

  virtual int GetNumMonitors() = 0;
  virtual gfx::Display GetMonitorNearestWindow(
      gfx::NativeView window) const = 0;
  virtual gfx::Display GetMonitorNearestPoint(
      const gfx::Point& point) const = 0;
  virtual gfx::Display GetPrimaryMonitor() const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_SCREEN_IMPL_H_
