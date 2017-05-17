// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_UI_DIRECT_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_UI_DIRECT_INPUT_STRATEGY_H_

#include "remoting/client/ui/input_strategy.h"

namespace remoting {

// This strategy directly translates all operations on the OpenGL view into
// corresponding operations on the desktop. It doesn't maintain the cursor
// positions separately -- the positions come from the location of the touch.
class DirectInputStrategy : public InputStrategy {
 public:
  DirectInputStrategy();
  ~DirectInputStrategy() override;

  // InputStrategy overrides.

  void HandlePinch(const ViewMatrix::Point& pivot,
                   float scale,
                   DesktopViewport* viewport) override;

  void HandlePan(const ViewMatrix::Vector2D& translation,
                 bool is_dragging_mode,
                 DesktopViewport* viewport) override;

  void TrackTouchInput(const ViewMatrix::Point& touch_point,
                       const DesktopViewport& viewport) override;

  ViewMatrix::Point GetCursorPosition() const override;

  ViewMatrix::Vector2D MapScreenVectorToDesktop(
      const ViewMatrix::Vector2D& delta,
      const DesktopViewport& viewport) const override;

  float GetFeedbackRadius(InputFeedbackType type) const override;

  bool IsCursorVisible() const override;

 private:
  ViewMatrix::Point cursor_position_{0.f, 0.f};

  // TouchInputStrategy is neither copyable nor movable.
  DirectInputStrategy(const DirectInputStrategy&) = delete;
  DirectInputStrategy& operator=(const DirectInputStrategy&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_UI_DIRECT_INPUT_STRATEGY_H_
