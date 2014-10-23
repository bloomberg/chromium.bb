// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_SCREEN_H_
#define UI_AURA_TEST_TEST_SCREEN_H_

#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Insets;
class Rect;
class Transform;
}

namespace aura {
class Window;
class WindowTreeHost;

// A minimal, testing Aura implementation of gfx::Screen.
class TestScreen : public gfx::Screen,
                   public WindowObserver {
 public:
  // Creates a gfx::Screen of the specified size. If no size is specified, then
  // creates a 800x600 screen. |size| is in physical pixels.
  static TestScreen* Create(const gfx::Size& size);
  // Creates a TestScreen that uses fullscreen for the display.
  static TestScreen* CreateFullscreen();
  virtual ~TestScreen();

  WindowTreeHost* CreateHostForPrimaryDisplay();

  void SetDeviceScaleFactor(float device_scale_fator);
  void SetDisplayRotation(gfx::Display::Rotation rotation);
  void SetUIScale(float ui_scale);
  void SetWorkAreaInsets(const gfx::Insets& insets);

 protected:
  gfx::Transform GetRotationTransform() const;
  gfx::Transform GetUIScaleTransform() const;

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) override;
  virtual void OnWindowDestroying(Window* window) override;

  // gfx::Screen overrides:
  virtual gfx::Point GetCursorScreenPoint() override;
  virtual gfx::NativeWindow GetWindowUnderCursor() override;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      override;
  virtual int GetNumDisplays() const override;
  virtual std::vector<gfx::Display> GetAllDisplays() const override;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const override;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  virtual gfx::Display GetPrimaryDisplay() const override;
  virtual void AddObserver(gfx::DisplayObserver* observer) override;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) override;

 private:
  explicit TestScreen(const gfx::Rect& screen_bounds);

  aura::WindowTreeHost* host_;

  gfx::Display display_;

  float ui_scale_;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace aura

#endif  // UI_AURA_TEST_TEST_SCREEN_H_
