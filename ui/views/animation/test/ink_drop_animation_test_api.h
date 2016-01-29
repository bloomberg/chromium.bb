// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropAnimation;

namespace test {

// Test API to provide internal access to an InkDropAnimation.
class InkDropAnimationTestApi {
 public:
  typedef InkDropAnimation::InkDropTransforms InkDropTransforms;
  typedef InkDropAnimation::PaintedShape PaintedShape;

  explicit InkDropAnimationTestApi(InkDropAnimation* ink_drop_animation);
  ~InkDropAnimationTestApi();

  // Disables the animation timers when |disable_timers| is true. This
  void SetDisableAnimationTimers(bool disable_timers);

  // Returns true if any animations are active.
  bool HasActiveAnimations() const;

  // Completes all animations for all the Layer's owned by the InkDropAnimation.
  void CompleteAnimations();

  // Wrapper functions the wrapped InkDropedAnimation:
  float GetCurrentOpacity() const;
  void CalculateCircleTransforms(const gfx::Size& size,
                                 InkDropTransforms* transforms_out) const;
  void CalculateRectTransforms(const gfx::Size& size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

 private:
  // Progresses all running LayerAnimationSequences by the given |duration|.
  // NOTE: This function will NOT progress LayerAnimationSequences that are
  // queued, only the running ones will be progressed.
  void StepAnimations(const base::TimeDelta& duration);

  // Get a list of all the LayerAnimator's used internally by the
  // InkDropAnimation.
  std::vector<ui::LayerAnimator*> GetLayerAnimators();
  std::vector<ui::LayerAnimator*> GetLayerAnimators() const;

  // The InkDropAnimation to provide internal access to.
  InkDropAnimation* ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
