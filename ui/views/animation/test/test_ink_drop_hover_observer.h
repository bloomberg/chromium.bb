// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOVER_OBSERVER_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOVER_OBSERVER_H_

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/ink_drop_hover_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/test_ink_drop_animation_observer_helper.h"

namespace views {
namespace test {

// Simple InkDropHoverObserver test double that tracks if InkDropHoverObserver
// methods are invoked and the parameters used for the last invocation.
class TestInkDropHoverObserver
    : public InkDropHoverObserver,
      public TestInkDropAnimationObserverHelper<InkDropHover::AnimationType> {
 public:
  TestInkDropHoverObserver();
  ~TestInkDropHoverObserver() override = default;

  void set_ink_drop_hover(InkDropHover* ink_drop_hover) {
    ink_drop_hover_ = ink_drop_hover;
  }

  // InkDropHoverObserver:
  void AnimationStarted(InkDropHover::AnimationType animation_type) override;
  void AnimationEnded(InkDropHover::AnimationType animation_type,
                      InkDropAnimationEndedReason reason) override;

 private:
  // The type this inherits from. Reduces verbosity in .cc file.
  using ObserverHelper =
      TestInkDropAnimationObserverHelper<InkDropHover::AnimationType>;

  // An InkDropHover to spy info from when notifications are handled.
  InkDropHover* ink_drop_hover_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHoverObserver);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOVER_OBSERVER_H_
