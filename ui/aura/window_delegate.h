// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_DELEGATE_H_
#define UI_AURA_WINDOW_DELEGATE_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Canvas;
class Point;
class Rect;
}

namespace aura {

class KeyEvent;
class MouseEvent;

// Delegate interface for aura::Window.
class WindowDelegate {
 public:
  // Called when the Window's position and/or size changes.
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) = 0;

  // Sent to the Window's delegate when the Window gains or loses focus.
  virtual void OnFocus() = 0;
  virtual void OnBlur() = 0;

  virtual bool OnKeyEvent(KeyEvent* event) = 0;

  // Returns the native cursor for the specified point, in window coordinates,
  // or NULL for the default cursor.
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) = 0;

  // Returns the non-client component (see hit_test.h) containing |point|, in
  // window coordinates.
  virtual int GetNonClientComponent(const gfx::Point& point) const = 0;

  virtual bool OnMouseEvent(MouseEvent* event) = 0;

  // Returns true if the window should be activated |event| is either the mouse
  // event supplied if the activation is the result of a mouse, or NULL if
  // activation is attempted for another reason.
  virtual bool ShouldActivate(MouseEvent* event) = 0;

  // Sent when the window is activated.
  virtual void OnActivated() = 0;

  // Sent when the window loses active status.
  virtual void OnLostActive() = 0;

  // Invoked when mouse capture is lost on the window.
  virtual void OnCaptureLost() = 0;

  // Asks the delegate to paint window contents into the supplied canvas.
  virtual void OnPaint(gfx::Canvas* canvas) = 0;

  // Called from Window's destructor before OnWindowDestroyed and before the
  // children have been destroyed.
  virtual void OnWindowDestroying() = 0;

  // Called when the Window has been destroyed (i.e. from its destructor). This
  // is called after OnWindowDestroying and after the children have been
  // deleted.
  // The delegate can use this as an opportunity to delete itself if necessary.
  virtual void OnWindowDestroyed() = 0;

 protected:
  virtual ~WindowDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_DELEGATE_H_
