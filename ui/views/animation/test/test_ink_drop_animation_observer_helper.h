// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_ANIMATION_OBSERVER_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_ANIMATION_OBSERVER_H_

#include <algorithm>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_animation_ended_reason.h"

namespace views {
namespace test {

// Context tracking helper that can be used with test implementations of
// ink drop animation observers.
template <typename ContextType>
class TestInkDropAnimationObserverHelper {
 public:
  TestInkDropAnimationObserverHelper()
      : last_animation_started_ordinal_(-1),
        last_animation_started_context_(),
        last_animation_ended_ordinal_(-1),
        last_animation_ended_context_(),
        last_animation_ended_reason_(InkDropAnimationEndedReason::SUCCESS) {}

  virtual ~TestInkDropAnimationObserverHelper() {}

  int last_animation_started_ordinal() const {
    return last_animation_started_ordinal_;
  }

  ContextType last_animation_started_context() const {
    return last_animation_started_context_;
  }

  int last_animation_ended_ordinal() const {
    return last_animation_ended_ordinal_;
  }

  ContextType last_animation_ended_context() const {
    return last_animation_ended_context_;
  }

  InkDropAnimationEndedReason last_animation_ended_reason() const {
    return last_animation_ended_reason_;
  }

  void OnAnimationStarted(ContextType context) {
    last_animation_started_context_ = context;
    last_animation_started_ordinal_ = GetNextOrdinal();
  }

  void OnAnimationEnded(ContextType context,
                        InkDropAnimationEndedReason reason) {
    last_animation_ended_context_ = context;
    last_animation_ended_ordinal_ = GetNextOrdinal();
    last_animation_ended_reason_ = reason;
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
  testing::AssertionResult AnimationHasStarted() {
    if (last_animation_started_ordinal() > 0) {
      return testing::AssertionSuccess()
             << "Animations were started at ordinal="
             << last_animation_started_ordinal() << ".";
    }
    return testing::AssertionFailure() << "Animations have not started.";
  }

  // Passes *_TRUE assertions when an InkDropAnimationStarted() event has NOT
  // been observed.
  testing::AssertionResult AnimationHasNotStarted() {
    if (last_animation_started_ordinal() < 0)
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Animations were started at ordinal="
                                       << last_animation_started_ordinal()
                                       << ".";
  }

  // Passes *_TRUE assertions when an InkDropAnimationEnded() event has been
  // observed.
  testing::AssertionResult AnimationHasEnded() {
    if (last_animation_ended_ordinal() > 0) {
      return testing::AssertionSuccess() << "Animations were ended at ordinal="
                                         << last_animation_ended_ordinal()
                                         << ".";
    }
    return testing::AssertionFailure() << "Animations have not ended.";
  }

  // Passes *_TRUE assertions when an InkDropAnimationEnded() event has NOT been
  // observed.
  testing::AssertionResult AnimationHasNotEnded() {
    if (last_animation_ended_ordinal() < 0)
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Animations were ended at ordinal="
                                       << last_animation_ended_ordinal() << ".";
  }

 private:
  // Returns the next event ordinal. The first returned ordinal will be 1.
  int GetNextOrdinal() const {
    return std::max(1, std::max(last_animation_started_ordinal_,
                                last_animation_ended_ordinal_) +
                           1);
  }

  // The ordinal time of the last AnimationStarted() call.
  int last_animation_started_ordinal_;

  // The |context| passed to the last call to AnimationStarted().
  ContextType last_animation_started_context_;

  // The ordinal time of the last AnimationEnded() call.
  int last_animation_ended_ordinal_;

  // The |context| passed to the last call to AnimationEnded().
  ContextType last_animation_ended_context_;

  InkDropAnimationEndedReason last_animation_ended_reason_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropAnimationObserverHelper);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_ANIMATION_OBSERVER_H_
