// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"

#include "ash/public/cpp/back_gesture_contextual_nudge_controller.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace ash {

TestBackGestureContextualNudgeDelegate::TestBackGestureContextualNudgeDelegate(
    BackGestureContextualNudgeController* controller)
    : controller_(controller) {}

TestBackGestureContextualNudgeDelegate::
    ~TestBackGestureContextualNudgeDelegate() = default;

void TestBackGestureContextualNudgeDelegate::MaybeStartTrackingNavigation(
    aura::Window* window) {
  if (window->type() == aura::client::WindowType::WINDOW_TYPE_NORMAL)
    controller_->NavigationEntryChanged(window);
}

}  // namespace ash
