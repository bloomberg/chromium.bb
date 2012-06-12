// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESKTOP_SCREEN_WIN_H_
#define UI_AURA_DESKTOP_DESKTOP_SCREEN_WIN_H_
#pragma once

#include "ui/aura/aura_export.h"
#include "ui/gfx/screen_impl.h"

namespace aura {

class AURA_EXPORT DesktopScreenWin : public gfx::ScreenImpl {
public:
  DesktopScreenWin();
  virtual ~DesktopScreenWin();

  // Overridden from gfx::ScreenImpl:
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPoint() OVERRIDE;
  virtual int GetNumMonitors() OVERRIDE;
  virtual gfx::Display GetMonitorNearestWindow(
      gfx::NativeView window) const OVERRIDE;
  virtual gfx::Display GetMonitorNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetPrimaryMonitor() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopScreenWin);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESKTOP_SCREEN_WIN_H_
