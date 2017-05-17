// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_UI_GESTURE_INTERPRETER_H_
#define REMOTING_CLIENT_UI_GESTURE_INTERPRETER_H_

#include <memory>

#include "remoting/client/ui/desktop_viewport.h"
#include "remoting/client/ui/fling_animation.h"
#include "remoting/client/ui/input_strategy.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

class ChromotingSession;
class RendererProxy;

// This is a class for interpreting a raw touch input into actions like moving
// the viewport and injecting mouse clicks.
class GestureInterpreter {
 public:
  enum GestureState { GESTURE_BEGAN, GESTURE_CHANGED, GESTURE_ENDED };

  GestureInterpreter(RendererProxy* renderer, ChromotingSession* input_stub);
  ~GestureInterpreter();

  // Coordinates of the OpenGL view surface will be used.

  // This can happen in conjunction with Pan().
  void Pinch(float pivot_x, float pivot_y, float scale);

  // Called whenever the user did a pan gesture. It can be one-finger pan, no
  // matter long-press in on or not, or two-finger pan in conjunction with pinch
  // and long-press. Two-finger pan without pinch is consider a scroll gesture.
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

  // Called during a two-finger scroll (panning without pinching) gesture.
  void Scroll(float x, float y, float dx, float dy);

  // Called when the user has just done a scroll gesture and the scroll gesture
  // still has some final velocity.
  void ScrollWithVelocity(float velocity_x, float velocity_y);

  // Called to process one animation frame.
  void ProcessAnimations();

  void OnSurfaceSizeChanged(int width, int height);
  void OnDesktopSizeChanged(int width, int height);

 private:
  void PanWithoutAbortAnimations(float translation_x, float translation_y);

  void ScrollWithoutAbortAnimations(float dx, float dy);

  void AbortAnimations();

  void InjectMouseClick(float x,
                        float y,
                        protocol::MouseEvent_MouseButton button);

  // Tracks the touch point and gets back the cursor position from the input
  // strategy.
  ViewMatrix::Point TrackAndGetPosition(float touch_x, float touch_y);

  // If the cursor is visible, send the cursor position from the input strategy
  // to the renderer.
  void SetCursorPositionOnRenderer();

  // Starts the given feedback at (cursor_x, cursor_y) if the feedback radius
  // is non-zero.
  void StartInputFeedback(float cursor_x,
                          float cursor_y,
                          InputStrategy::InputFeedbackType feedback_type);

  std::unique_ptr<InputStrategy> input_strategy_;
  DesktopViewport viewport_;
  RendererProxy* renderer_;
  ChromotingSession* input_stub_;
  bool is_dragging_mode_ = false;

  FlingAnimation pan_animation_;
  FlingAnimation scroll_animation_;

  // GestureInterpreter is neither copyable nor movable.
  GestureInterpreter(const GestureInterpreter&) = delete;
  GestureInterpreter& operator=(const GestureInterpreter&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_UI_GESTURE_INTERPRETER_H_
