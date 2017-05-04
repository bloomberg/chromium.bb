// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_GESTURE_INTERPRETER_H_
#define REMOTING_CLIENT_GESTURE_INTERPRETER_H_

#include "remoting/client/desktop_viewport.h"

namespace remoting {

// This is a class for interpreting a raw touch input into actions like moving
// the viewport and injecting mouse clicks.
class GestureInterpreter {
 public:
  GestureInterpreter(
      const DesktopViewport::TransformationCallback& on_transformation_changed);
  ~GestureInterpreter();

  // Coordinates of the OpenGL view surface will be used.
  void Pinch(float pivot_x, float pivot_y, float scale);
  void Pan(float translation_x, float translation_y);
  void Tap(float x, float y);
  void LongPress(float x, float y);

  void OnSurfaceSizeChanged(int width, int height);
  void OnDesktopSizeChanged(int width, int height);

 private:
  DesktopViewport viewport_;

  // GestureInterpreter is neither copyable nor movable.
  GestureInterpreter(const GestureInterpreter&) = delete;
  GestureInterpreter& operator=(const GestureInterpreter&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_GESTURE_INTERPRETER_H_
