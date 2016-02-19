// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_ANIMATION_OBSERVER_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_ANIMATION_OBSERVER_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/animation/ink_drop_state.h"

namespace views {
class InkDropAnimation;

namespace test {

// Simple InkDropAnimationObserver test double that tracks if
// InkDropAnimationObserver methods are invoked and the parameters used for the
// last invocation.
class TestInkDropAnimationObserver : public InkDropAnimationObserver {
 public:
  TestInkDropAnimationObserver();
  ~TestInkDropAnimationObserver() override;

  void set_ink_drop_animation(InkDropAnimation* ink_drop_animation) {
    ink_drop_animation_ = ink_drop_animation;
  }

  int last_animation_started_ordinal() const {
    return last_animation_started_ordinal_;
  }

  int last_animation_ended_ordinal() const {
    return last_animation_ended_ordinal_;
  }

  InkDropState last_animation_state_started() const {
    return last_animation_state_started_;
  }

  InkDropState last_animation_state_ended() const {
    return last_animation_state_ended_;
  }

  InkDropAnimationEndedReason last_animation_ended_reason() const {
    return last_animation_ended_reason_;
  }

  InkDropState target_state_at_last_animation_started() const {
    return target_state_at_last_animation_started_;
  }

  InkDropState target_state_at_last_animation_ended() const {
    return target_state_at_last_animation_ended_;
  }

  //
  // Collection of assertion predicates to be used with GTest test assertions.
  // i.e. EXPECT_TRUE/EXPECT_FALSE and the ASSERT_ counterparts.
  //
  // Example:
  //
  //   TestInkDropAnimationObserver observer;
  //   observer.set_ink_drop_animation(ink_drop_animation);
  //   EXPECT_TRUE(observer.AnimationHasNotStarted());
  //

  // Passes *_TRUE assertions when an InkDropAnimationStarted() event has been
  // observed.
  testing::AssertionResult AnimationHasStarted();

  // Passes *_TRUE assertions when an InkDropAnimationStarted() event has NOT
  // been observed.
  testing::AssertionResult AnimationHasNotStarted();

  // Passes *_TRUE assertions when an InkDropAnimationEnded() event has been
  // observed.
  testing::AssertionResult AnimationHasEnded();

  // Passes *_TRUE assertions when an InkDropAnimationEnded() event has NOT been
  // observed.
  testing::AssertionResult AnimationHasNotEnded();

  // InkDropAnimation:
  void InkDropAnimationStarted(InkDropState ink_drop_state) override;
  void InkDropAnimationEnded(InkDropState ink_drop_state,
                             InkDropAnimationEndedReason reason) override;

 private:
  // Returns the next event ordinal. The first returned ordinal will be 1.
  int GetNextOrdinal() const;

  // The ordinal time of the last InkDropAnimationStarted() event.
  int last_animation_started_ordinal_;

  // The ordinal time of the last InkDropAnimationended() event.
  int last_animation_ended_ordinal_;

  // The |ink_drop_state| parameter used for the last invocation of
  // InkDropAnimationStarted(). Only valid if |animation_started_| is true.
  InkDropState last_animation_state_started_;

  // The |ink_drop_state| parameter used for the last invocation of
  // InkDropAnimationEnded(). Only valid if |animation_ended_| is true.
  InkDropState last_animation_state_ended_;

  // The |reason| parameter used for the last invocation of
  // InkDropAnimationEnded(). Only valid if |animation_ended_| is true.
  InkDropAnimationEndedReason last_animation_ended_reason_;

  // The value of InkDropAnimation::GetTargetInkDropState() the last time an
  // InkDropAnimationStarted() event was handled. This is only valid if
  // |ink_drop_animation_| is not null.
  InkDropState target_state_at_last_animation_started_;

  // The value of InkDropAnimation::GetTargetInkDropState() the last time an
  // InkDropAnimationEnded() event was handled. This is only valid if
  // |ink_drop_animation_| is not null.
  InkDropState target_state_at_last_animation_ended_;

  // An InkDropAnimation to spy info from when notifications are handled.
  InkDropAnimation* ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropAnimationObserver);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_ANIMATION_OBSERVER_H_
