// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gesture_interpreter.h"
#include "base/logging.h"
#include "remoting/client/direct_input_strategy.h"

namespace remoting {

GestureInterpreter::GestureInterpreter(
    const DesktopViewport::TransformationCallback& on_transformation_changed,
    ChromotingSession* input_stub)
    : input_stub_(input_stub) {
  viewport_.RegisterOnTransformationChangedCallback(on_transformation_changed,
                                                    true);

  // TODO(yuweih): This should be configurable.
  input_strategy_.reset(new DirectInputStrategy());
}

GestureInterpreter::~GestureInterpreter() {}

void GestureInterpreter::Pinch(float pivot_x, float pivot_y, float scale) {
  input_strategy_->HandlePinch(pivot_x, pivot_y, scale, &viewport_);
}

void GestureInterpreter::Pan(float translation_x, float translation_y) {
  // TODO(yuweih): Pan deceleration animation.
  input_strategy_->HandlePan(translation_x, translation_y, is_dragging_mode_,
                             &viewport_);
}

void GestureInterpreter::Tap(float x, float y) {
  float cursor_x, cursor_y;
  input_strategy_->FindCursorPositions(x, y, viewport_, &cursor_x, &cursor_y);
  InjectMouseClick(cursor_x, cursor_y,
                   protocol::MouseEvent_MouseButton_BUTTON_LEFT);
}

void GestureInterpreter::TwoFingerTap(float x, float y) {
  float cursor_x, cursor_y;
  input_strategy_->FindCursorPositions(x, y, viewport_, &cursor_x, &cursor_y);
  InjectMouseClick(cursor_x, cursor_y,
                   protocol::MouseEvent_MouseButton_BUTTON_RIGHT);
}

void GestureInterpreter::LongPress(float x, float y, GestureState state) {
  float cursor_x, cursor_y;
  input_strategy_->FindCursorPositions(x, y, viewport_, &cursor_x, &cursor_y);

  is_dragging_mode_ = state != GESTURE_ENDED;
  input_stub_->SendMouseEvent(cursor_x, cursor_y,
                              protocol::MouseEvent_MouseButton_BUTTON_LEFT,
                              is_dragging_mode_);
}

void GestureInterpreter::InjectMouseClick(
    float x,
    float y,
    protocol::MouseEvent_MouseButton button) {
  input_stub_->SendMouseEvent(x, y, button, true);
  input_stub_->SendMouseEvent(x, y, button, false);
}

void GestureInterpreter::OnSurfaceSizeChanged(int width, int height) {
  viewport_.SetSurfaceSize(width, height);
}

void GestureInterpreter::OnDesktopSizeChanged(int width, int height) {
  viewport_.SetDesktopSize(width, height);
}

}  // namespace remoting
