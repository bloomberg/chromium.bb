// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_animation_observer.h"

#include "ui/views/animation/ink_drop_animation.h"

namespace views {
namespace test {

TestInkDropAnimationObserver::TestInkDropAnimationObserver()
    : last_animation_started_ordinal_(-1),
      last_animation_ended_ordinal_(-1),
      last_animation_state_started_(InkDropState::HIDDEN),
      last_animation_state_ended_(InkDropState::HIDDEN),
      last_animation_ended_reason_(InkDropAnimationEndedReason::SUCCESS),
      target_state_at_last_animation_started_(InkDropState::HIDDEN),
      target_state_at_last_animation_ended_(InkDropState::HIDDEN) {}

TestInkDropAnimationObserver::~TestInkDropAnimationObserver() {}

testing::AssertionResult TestInkDropAnimationObserver::AnimationHasStarted() {
  if (last_animation_started_ordinal() > 0) {
    return testing::AssertionSuccess() << "Animations were started at ordinal="
                                       << last_animation_started_ordinal()
                                       << ".";
  }
  return testing::AssertionFailure() << "Animations have not started.";
}

testing::AssertionResult
TestInkDropAnimationObserver::AnimationHasNotStarted() {
  if (last_animation_started_ordinal() < 0)
    return testing::AssertionSuccess();
  return testing::AssertionFailure() << "Animations were started at ordinal="
                                     << last_animation_started_ordinal() << ".";
}

testing::AssertionResult TestInkDropAnimationObserver::AnimationHasEnded() {
  if (last_animation_ended_ordinal() > 0) {
    return testing::AssertionSuccess() << "Animations were ended at ordinal="
                                       << last_animation_ended_ordinal() << ".";
  }
  return testing::AssertionFailure() << "Animations have not ended.";
}

testing::AssertionResult TestInkDropAnimationObserver::AnimationHasNotEnded() {
  if (last_animation_ended_ordinal() < 0)
    return testing::AssertionSuccess();
  return testing::AssertionFailure() << "Animations were ended at ordinal="
                                     << last_animation_ended_ordinal() << ".";
}

void TestInkDropAnimationObserver::InkDropAnimationStarted(
    InkDropState ink_drop_state) {
  last_animation_started_ordinal_ = GetNextOrdinal();
  last_animation_state_started_ = ink_drop_state;
  if (ink_drop_animation_) {
    target_state_at_last_animation_started_ =
        ink_drop_animation_->target_ink_drop_state();
  }
}

void TestInkDropAnimationObserver::InkDropAnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  last_animation_ended_ordinal_ = GetNextOrdinal();
  last_animation_state_ended_ = ink_drop_state;
  last_animation_ended_reason_ = reason;
  if (ink_drop_animation_) {
    target_state_at_last_animation_ended_ =
        ink_drop_animation_->target_ink_drop_state();
  }
}

int TestInkDropAnimationObserver::GetNextOrdinal() const {
  return std::max(1, std::max(last_animation_started_ordinal_,
                              last_animation_ended_ordinal_) +
                         1);
}

}  // namespace test
}  // namespace views
