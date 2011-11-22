// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_DELEGATE_H_
#define UI_AURA_WINDOW_DELEGATE_H_
#pragma once

#include "ui/aura/aura_export.h"
#include "ui/base/events.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Canvas;
class Point;
class Rect;
class Size;
}

namespace aura {

class Event;
class KeyEvent;
class MouseEvent;
class TouchEvent;

// Delegate interface for aura::Window.
class AURA_EXPORT WindowDelegate {
 public:
  // Returns the window's minimum size, or size 0,0 if there is no limit.
  virtual gfx::Size GetMinimumSize() const = 0;

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

  virtual ui::TouchStatus OnTouchEvent(TouchEvent* event) = 0;

  // Returns true if the window should be activated. |event| is either the mouse
  // event supplied if the activation is the result of a mouse, or the touch
  // event if the activation is the result of a touch, or NULL if activation is
  // attempted for another reason.
  virtual bool ShouldActivate(Event* event) = 0;

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

  // Called when the visibility of a Window changed.
  virtual void OnWindowVisibilityChanged(bool visible) = 0;

 protected:
  virtual ~WindowDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_DELEGATE_H_
