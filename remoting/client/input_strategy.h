// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_INPUT_STRATEGY_H_

namespace remoting {

class DesktopViewport;

// This is an interface used by GestureInterpreter to customize the way gestures
// are handled.
class InputStrategy {
 public:
  virtual ~InputStrategy() {}

  // Called when the GestureInterpreter receives a pinch gesture. The
  // implementation is responsible for modifying the viewport and observing the
  // change.
  virtual void HandlePinch(float pivot_x,
                           float pivot_y,
                           float scale,
                           DesktopViewport* viewport) = 0;

  // Called when the GestureInterpreter receives a pan gesture. The
  // implementation is responsible for modifying the viewport and observing the
  // change.
  // is_dragging_mode: true if the user is trying to drag something while
  //     panning on the screen.
  virtual void HandlePan(float translation_x,
                         float translation_y,
                         bool is_dragging_mode,
                         DesktopViewport* viewport) = 0;

  // Called when a gesture is done at location (touch_x, touch_y) and the
  // GestureInterpreter needs to get back the cursor position to inject mouse
  // the mouse event.
  virtual void FindCursorPositions(float touch_x,
                                   float touch_y,
                                   const DesktopViewport& viewport,
                                   float* cursor_x,
                                   float* cursor_y) = 0;

  // Returns true if the input strategy maintains a visible cursor on the
  // desktop.
  virtual bool IsCursorVisible() const = 0;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_STRATEGY_H_
