// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/direct_input_strategy.h"
#include "remoting/client/desktop_viewport.h"

namespace remoting {

DirectInputStrategy::DirectInputStrategy() {}

DirectInputStrategy::~DirectInputStrategy() {}

void DirectInputStrategy::HandlePinch(float pivot_x,
                                      float pivot_y,
                                      float scale,
                                      DesktopViewport* viewport) {
  viewport->ScaleDesktop(pivot_x, pivot_y, scale);
}

void DirectInputStrategy::HandlePan(float translation_x,
                                    float translation_y,
                                    bool is_dragging_mode,
                                    DesktopViewport* viewport) {
  if (is_dragging_mode) {
    // If the user is dragging something, we should synchronize the movement
    // with the object that the user is trying to move on the desktop, rather
    // than moving the desktop around.
    ViewMatrix::Vector2D viewport_movement =
        viewport->GetTransformation().Invert().MapVector(
            {translation_x, translation_y});
    viewport->MoveViewport(viewport_movement.x, viewport_movement.y);
    return;
  }

  viewport->MoveDesktop(translation_x, translation_y);
}

void DirectInputStrategy::FindCursorPositions(float touch_x,
                                              float touch_y,
                                              const DesktopViewport& viewport,
                                              float* cursor_x,
                                              float* cursor_y) {
  ViewMatrix::Point cursor_position =
      viewport.GetTransformation().Invert().MapPoint({touch_x, touch_y});
  *cursor_x = cursor_position.x;
  *cursor_y = cursor_position.y;
}

bool DirectInputStrategy::IsCursorVisible() const {
  return false;
}

}  // namespace remoting
