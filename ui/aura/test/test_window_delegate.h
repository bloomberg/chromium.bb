// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
#define UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_delegate.h"

namespace aura {
namespace test {

// WindowDelegate implementation with all methods stubbed out.
class TestWindowDelegate : public WindowDelegate {
 public:
  TestWindowDelegate();
  virtual ~TestWindowDelegate();

  void set_window_component(int window_component) {
    window_component_ = window_component;
  }

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus OnGestureEvent(GestureEvent* event) OVERRIDE;
  virtual bool CanFocus() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void OnWindowDestroying() OVERRIDE;
  virtual void OnWindowDestroyed() OVERRIDE;
  virtual void OnWindowVisibilityChanged(bool visible) OVERRIDE;

 private:
  int window_component_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

// A simple WindowDelegate implementation for these tests. It owns itself
// (deletes itself when the Window it is attached to is destroyed).
class ColorTestWindowDelegate : public TestWindowDelegate {
 public:
  explicit ColorTestWindowDelegate(SkColor color);
  virtual ~ColorTestWindowDelegate();

  ui::KeyboardCode last_key_code() const { return last_key_code_; }

  // Overridden from TestWindowDelegate:
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE;
  virtual void OnWindowDestroyed() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  SkColor color_;
  ui::KeyboardCode last_key_code_;

  DISALLOW_COPY_AND_ASSIGN(ColorTestWindowDelegate);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
