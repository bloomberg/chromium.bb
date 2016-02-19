// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropAnimation;

namespace test {

// Base Test API used by test fixtures to validate all concrete implementations
// of the InkDropAnimation class.
class InkDropAnimationTestApi {
 public:
  explicit InkDropAnimationTestApi(InkDropAnimation* ink_drop_animation);
  virtual ~InkDropAnimationTestApi();

  // Disables the animation timers when |disable_timers| is true.
  void SetDisableAnimationTimers(bool disable_timers);

  // Returns true if any animations are active.
  bool HasActiveAnimations() const;

  // Completes all animations for all the Layer's owned by the InkDropAnimation.
  void CompleteAnimations();

  // Gets the opacity of the ink drop.
  virtual float GetCurrentOpacity() const = 0;

 protected:
  InkDropAnimation* ink_drop_animation() {
    return static_cast<const InkDropAnimationTestApi*>(this)
        ->ink_drop_animation();
  }

  InkDropAnimation* ink_drop_animation() const { return ink_drop_animation_; }

  // Get a list of all the LayerAnimator's used internally by the
  // InkDropAnimation.
  std::vector<ui::LayerAnimator*> GetLayerAnimators();
  virtual std::vector<ui::LayerAnimator*> GetLayerAnimators() const;

 private:
  // Progresses all running LayerAnimationSequences by the given |duration|.
  //
  // NOTE: This function will NOT progress LayerAnimationSequences that are
  // queued, only the running ones will be progressed.
  void StepAnimations(const base::TimeDelta& duration);

  // The InkDropedAnimation to provide internal access to.
  InkDropAnimation* ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
