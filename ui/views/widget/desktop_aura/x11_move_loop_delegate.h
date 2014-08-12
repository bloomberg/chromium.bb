// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_MOVE_LOOP_DELEGATE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_MOVE_LOOP_DELEGATE_H_

#include <X11/Xlib.h>

namespace views {

// Receives mouse events while the X11MoveLoop is tracking a drag.
class X11MoveLoopDelegate {
 public:
  // Called when we receive a motion event.
  virtual void OnMouseMovement(XMotionEvent* event) = 0;

  // Called when the mouse button is released.
  virtual void OnMouseReleased() = 0;

  // Called when the user has released the mouse button. The move loop will
  // release the grab after this has been called.
  virtual void OnMoveLoopEnded() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_MOVE_LOOP_DELEGATE_H_
