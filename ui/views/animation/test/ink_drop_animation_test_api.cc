// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_animation_test_api.h"

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/animation/ink_drop_animation.h"

namespace views {
namespace test {

InkDropAnimationTestApi::InkDropAnimationTestApi(
    InkDropAnimation* ink_drop_animation)
    : ink_drop_animation_(ink_drop_animation) {}

InkDropAnimationTestApi::~InkDropAnimationTestApi() {}

void InkDropAnimationTestApi::SetDisableAnimationTimers(bool disable_timers) {
  for (ui::LayerAnimator* animator : GetLayerAnimators())
    animator->set_disable_timer_for_test(disable_timers);
}

bool InkDropAnimationTestApi::HasActiveAnimations() const {
  for (ui::LayerAnimator* animator : GetLayerAnimators()) {
    if (animator->is_animating())
      return true;
  }
  return false;
}

void InkDropAnimationTestApi::CompleteAnimations() {
  while (HasActiveAnimations()) {
    // StepAnimations() will only progress the current running animations. Thus
    // each queued animation will require at least one 'Step' call and we cannot
    // just use a large duration here.
    StepAnimations(base::TimeDelta::FromMilliseconds(20));
  }
}

std::vector<ui::LayerAnimator*> InkDropAnimationTestApi::GetLayerAnimators() {
  return static_cast<const InkDropAnimationTestApi*>(this)->GetLayerAnimators();
}

std::vector<ui::LayerAnimator*> InkDropAnimationTestApi::GetLayerAnimators()
    const {
  return std::vector<ui::LayerAnimator*>();
}

void InkDropAnimationTestApi::StepAnimations(const base::TimeDelta& duration) {
  for (ui::LayerAnimator* animator : GetLayerAnimators()) {
    ui::LayerAnimatorTestController controller(animator);
    controller.StartThreadedAnimationsIfNeeded();
    controller.Step(duration);
  }
}

}  // namespace test
}  // namespace views
