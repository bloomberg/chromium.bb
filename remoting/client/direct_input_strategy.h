// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SIMULATED_TOUCH_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_SIMULATED_TOUCH_INPUT_STRATEGY_H_

#include "remoting/client/input_strategy.h"

namespace remoting {

// This strategy directly translates all operations on the OpenGL view into
// corresponding operations on the desktop. It doesn't maintain the cursor
// positions separately -- the positions come from the location of the touch.
class DirectInputStrategy : public InputStrategy {
 public:
  DirectInputStrategy();
  ~DirectInputStrategy() override;

  // InputStrategy overrides.

  void HandlePinch(float pivot_x,
                   float pivot_y,
                   float scale,
                   DesktopViewport* viewport) override;

  void HandlePan(float translation_x,
                 float translation_y,
                 bool is_dragging_mode,
                 DesktopViewport* viewport) override;

  void FindCursorPositions(float touch_x,
                           float touch_y,
                           const DesktopViewport& viewport,
                           float* cursor_x,
                           float* cursor_y) override;

  bool IsCursorVisible() const override;

 private:
  // TouchInputStrategy is neither copyable nor movable.
  DirectInputStrategy(const DirectInputStrategy&) = delete;
  DirectInputStrategy& operator=(const DirectInputStrategy&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_SIMULATED_TOUCH_INPUT_STRATEGY_H_
