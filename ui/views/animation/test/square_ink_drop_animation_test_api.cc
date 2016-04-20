// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/square_ink_drop_animation_test_api.h"

#include <vector>

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/animation/ink_drop_animation.h"

namespace views {
namespace test {

SquareInkDropAnimationTestApi::SquareInkDropAnimationTestApi(
    SquareInkDropAnimation* ink_drop_animation)
    : InkDropAnimationTestApi(ink_drop_animation) {}

SquareInkDropAnimationTestApi::~SquareInkDropAnimationTestApi() {}

void SquareInkDropAnimationTestApi::CalculateCircleTransforms(
    const gfx::Size& size,
    InkDropTransforms* transforms_out) const {
  ink_drop_animation()->CalculateCircleTransforms(size, transforms_out);
}
void SquareInkDropAnimationTestApi::CalculateRectTransforms(
    const gfx::Size& size,
    float corner_radius,
    InkDropTransforms* transforms_out) const {
  ink_drop_animation()->CalculateRectTransforms(size, corner_radius,
                                                transforms_out);
}

float SquareInkDropAnimationTestApi::GetCurrentOpacity() const {
  return ink_drop_animation()->GetCurrentOpacity();
}

std::vector<ui::LayerAnimator*>
SquareInkDropAnimationTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators =
      InkDropAnimationTestApi::GetLayerAnimators();
  animators.push_back(ink_drop_animation()->GetRootLayer()->GetAnimator());
  for (int i = 0; i < SquareInkDropAnimation::PAINTED_SHAPE_COUNT; ++i)
    animators.push_back(
        ink_drop_animation()->painted_layers_[i]->GetAnimator());
  return animators;
}

}  // namespace test
}  // namespace views
