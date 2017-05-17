// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ui/direct_input_strategy.h"
#include "remoting/client/ui/desktop_viewport.h"

namespace remoting {

namespace {

const float kTapFeedbackRadius = 25.f;
const float kLongPressFeedbackRadius = 55.f;

}  // namespace

DirectInputStrategy::DirectInputStrategy() {}

DirectInputStrategy::~DirectInputStrategy() {}

void DirectInputStrategy::HandlePinch(const ViewMatrix::Point& pivot,
                                      float scale,
                                      DesktopViewport* viewport) {
  viewport->ScaleDesktop(pivot.x, pivot.y, scale);
}

void DirectInputStrategy::HandlePan(const ViewMatrix::Vector2D& translation,
                                    bool is_dragging_mode,
                                    DesktopViewport* viewport) {
  if (is_dragging_mode) {
    // If the user is dragging something, we should synchronize the movement
    // with the object that the user is trying to move on the desktop, rather
    // than moving the desktop around.
    ViewMatrix::Vector2D viewport_movement =
        viewport->GetTransformation().Invert().MapVector(translation);
    viewport->MoveViewport(viewport_movement.x, viewport_movement.y);
    return;
  }

  viewport->MoveDesktop(translation.x, translation.y);
}

void DirectInputStrategy::TrackTouchInput(const ViewMatrix::Point& touch_point,
                                          const DesktopViewport& viewport) {
  cursor_position_ =
      viewport.GetTransformation().Invert().MapPoint(touch_point);
}

ViewMatrix::Point DirectInputStrategy::GetCursorPosition() const {
  return cursor_position_;
}

ViewMatrix::Vector2D DirectInputStrategy::MapScreenVectorToDesktop(
    const ViewMatrix::Vector2D& delta,
    const DesktopViewport& viewport) const {
  return viewport.GetTransformation().Invert().MapVector(delta);
}

float DirectInputStrategy::GetFeedbackRadius(InputFeedbackType type) const {
  switch (type) {
    case InputFeedbackType::TAP_FEEDBACK:
      return kTapFeedbackRadius;
    case InputFeedbackType::LONG_PRESS_FEEDBACK:
      return kLongPressFeedbackRadius;
  }
  NOTREACHED();
  return 0.f;
}

bool DirectInputStrategy::IsCursorVisible() const {
  return false;
}

}  // namespace remoting
