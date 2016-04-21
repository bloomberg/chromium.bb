// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_animation_observer.h"

#include "ui/views/animation/ink_drop_animation.h"

namespace views {
namespace test {

TestInkDropAnimationObserver::TestInkDropAnimationObserver()
    : target_state_at_last_animation_started_(InkDropState::HIDDEN),
      target_state_at_last_animation_ended_(InkDropState::HIDDEN) {}

TestInkDropAnimationObserver::~TestInkDropAnimationObserver() {}

void TestInkDropAnimationObserver::AnimationStarted(
    InkDropState ink_drop_state) {
  ObserverHelper::OnAnimationStarted(ink_drop_state);
  if (ink_drop_animation_) {
    target_state_at_last_animation_started_ =
        ink_drop_animation_->target_ink_drop_state();
  }
}

void TestInkDropAnimationObserver::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  ObserverHelper::OnAnimationEnded(ink_drop_state, reason);
  if (ink_drop_animation_) {
    target_state_at_last_animation_ended_ =
        ink_drop_animation_->target_ink_drop_state();
  }
}

}  // namespace test
}  // namespace views
