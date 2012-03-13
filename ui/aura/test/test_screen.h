// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_SCREEN_H_
#define UI_AURA_TEST_TEST_SCREEN_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

namespace aura {
class RootWindow;

// A minimal, testing Aura implementation of gfx::Screen.
class TestScreen : public gfx::Screen {
 public:
  explicit TestScreen(aura::RootWindow* root_window);
  virtual ~TestScreen();

 protected:
  virtual gfx::Point GetCursorScreenPointImpl() OVERRIDE;
  virtual gfx::Rect GetMonitorWorkAreaNearestWindowImpl(
      gfx::NativeView view) OVERRIDE;
  virtual gfx::Rect GetMonitorAreaNearestWindowImpl(
      gfx::NativeView view) OVERRIDE;
  virtual gfx::Rect GetMonitorWorkAreaNearestPointImpl(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::Rect GetMonitorAreaNearestPointImpl(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPointImpl() OVERRIDE;
  virtual gfx::Size GetPrimaryMonitorSizeImpl() OVERRIDE;
  virtual int GetNumMonitorsImpl() OVERRIDE;

 private:
  // We currently support only one monitor. These two methods return the bounds
  // and work area.
  gfx::Rect GetBounds();

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace aura

#endif  // UI_AURA_TEST_TEST_SCREEN_H_
