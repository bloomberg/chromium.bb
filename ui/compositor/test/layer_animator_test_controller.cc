// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/layer_animation_sequence.h"

namespace ui {

LayerAnimatorTestController::LayerAnimatorTestController(
    scoped_refptr<LayerAnimator> animator)
    : animator_(animator) {
}

LayerAnimatorTestController::~LayerAnimatorTestController() {
}

LayerAnimationSequence* LayerAnimatorTestController::GetRunningSequence(
    LayerAnimationElement::AnimatableProperty property) {
  LayerAnimator::RunningAnimation* running_animation =
      animator_->GetRunningAnimation(property);
  if (running_animation)
    return running_animation->sequence();
  else
    return NULL;
}

void LayerAnimatorTestController::StartThreadedAnimationsIfNeeded() {
  LayerAnimationSequence* sequence =
      GetRunningSequence(LayerAnimationElement::OPACITY);

  if (!sequence)
    return;

  LayerAnimationElement* element = sequence->CurrentElement();
  if (element->properties().find(LayerAnimationElement::OPACITY) ==
      element->properties().end())
    return;

  if (!element->Started() ||
      element->effective_start_time() != base::TimeTicks())
    return;

  animator_->OnThreadedAnimationStarted(cc::AnimationEvent(
      cc::AnimationEvent::Started,
      0,
      element->animation_group_id(),
      cc::Animation::Opacity,
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF()));
}

}  // namespace ui
