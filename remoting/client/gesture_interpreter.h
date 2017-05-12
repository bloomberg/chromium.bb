// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_GESTURE_INTERPRETER_H_
#define REMOTING_CLIENT_GESTURE_INTERPRETER_H_

#include <memory>

#include "remoting/client/chromoting_session.h"
#include "remoting/client/desktop_viewport.h"
#include "remoting/client/fling_animation.h"
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

  // This can happen in conjunction with Pan().
  void Pinch(float pivot_x, float pivot_y, float scale);

  // Called whenever the user did a pan gesture. It can be one-finger pan, no
  // matter long-press in on or not, or two-finger pan in conjunction with the
  // pinch gesture. Two-finger pan without pinch is consider a scroll gesture.
  void Pan(float translation_x, float translation_y);

  // Called when the user did a one-finger tap.
  void Tap(float x, float y);

  void TwoFingerTap(float x, float y);

  // Caller is expected to call both Pan() and LongPress() when long-press is in
  // progress.
  void LongPress(float x, float y, GestureState state);

  // Called when the user has just done a one-finger pan (no long-press or
  // pinching) and the pan gesture still has some final velocity.
  void OneFingerFling(float velocity_x, float velocity_y);

  // Called to process one animation frame.
  void ProcessAnimations();

  void OnSurfaceSizeChanged(int width, int height);
  void OnDesktopSizeChanged(int width, int height);

 private:
  void PanWithoutAbortAnimations(float translation_x, float translation_y);

  void AbortAnimations();

  void InjectMouseClick(float x,
                        float y,
                        protocol::MouseEvent_MouseButton button);

  std::unique_ptr<InputStrategy> input_strategy_;
  DesktopViewport viewport_;
  ChromotingSession* input_stub_;
  bool is_dragging_mode_ = false;

  FlingAnimation pan_animation_;

  // GestureInterpreter is neither copyable nor movable.
  GestureInterpreter(const GestureInterpreter&) = delete;
  GestureInterpreter& operator=(const GestureInterpreter&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_GESTURE_INTERPRETER_H_
