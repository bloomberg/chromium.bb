// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_GESTURE_INTERPRETER_H_
#define REMOTING_CLIENT_GESTURE_INTERPRETER_H_

#include <memory>

#include "remoting/client/chromoting_session.h"
#include "remoting/client/desktop_viewport.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

class InputStrategy;

// This is a class for interpreting a raw touch input into actions like moving
// the viewport and injecting mouse clicks.
class GestureInterpreter {
 public:
  enum GestureState { GESTURE_BEGAN, GESTURE_CHANGED, GESTURE_ENDED };

  GestureInterpreter(
      const DesktopViewport::TransformationCallback& on_transformation_changed,
      ChromotingSession* input_stub);
  ~GestureInterpreter();

  // Coordinates of the OpenGL view surface will be used.
  void Pinch(float pivot_x, float pivot_y, float scale);
  void Pan(float translation_x, float translation_y);
  void Tap(float x, float y);
  void TwoFingerTap(float x, float y);

  // Caller is expected to call both Pan() and LongPress() when long-press is in
  // progress.
  void LongPress(float x, float y, GestureState state);

  void OnSurfaceSizeChanged(int width, int height);
  void OnDesktopSizeChanged(int width, int height);

 private:
  void InjectMouseClick(float x,
                        float y,
                        protocol::MouseEvent_MouseButton button);

  std::unique_ptr<InputStrategy> input_strategy_;
  DesktopViewport viewport_;
  ChromotingSession* input_stub_;
  bool is_dragging_mode_ = false;

  // GestureInterpreter is neither copyable nor movable.
  GestureInterpreter(const GestureInterpreter&) = delete;
  GestureInterpreter& operator=(const GestureInterpreter&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_GESTURE_INTERPRETER_H_
