// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_SCREEN_H_
#define UI_AURA_TEST_TEST_SCREEN_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/gfx/screen_impl.h"

namespace aura {
class RootWindow;

// A minimal, testing Aura implementation of gfx::Screen.
class TestScreen : public gfx::ScreenImpl {
 public:
  explicit TestScreen(aura::RootWindow* root_window);
  virtual ~TestScreen();

 protected:
  // gfx::ScreenImpl overrides:
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPoint() OVERRIDE;
  virtual int GetNumMonitors() OVERRIDE;
  virtual gfx::Monitor GetMonitorNearestWindow(
      gfx::NativeView view) const OVERRIDE;
  virtual gfx::Monitor GetMonitorNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Monitor GetPrimaryMonitor() const OVERRIDE;

 private:
  gfx::Monitor GetMonitor() const;

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace aura

#endif  // UI_AURA_TEST_TEST_SCREEN_H_
