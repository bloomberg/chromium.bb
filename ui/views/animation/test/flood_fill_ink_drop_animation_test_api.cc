// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/flood_fill_ink_drop_animation_test_api.h"

#include <vector>

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/animation/ink_drop_animation.h"

namespace views {
namespace test {

FloodFillInkDropAnimationTestApi::FloodFillInkDropAnimationTestApi(
    FloodFillInkDropAnimation* ink_drop_animation)
    : InkDropAnimationTestApi(ink_drop_animation) {}

FloodFillInkDropAnimationTestApi::~FloodFillInkDropAnimationTestApi() {}

gfx::Transform FloodFillInkDropAnimationTestApi::CalculateTransform(
    float target_radius) const {
  return ink_drop_animation()->CalculateTransform(target_radius);
}

float FloodFillInkDropAnimationTestApi::GetCurrentOpacity() const {
  return ink_drop_animation()->root_layer_.opacity();
}

std::vector<ui::LayerAnimator*>
FloodFillInkDropAnimationTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators =
      InkDropAnimationTestApi::GetLayerAnimators();
  animators.push_back(ink_drop_animation()->GetRootLayer()->GetAnimator());
  animators.push_back(ink_drop_animation()->painted_layer_.GetAnimator());
  return animators;
}

}  // namespace test
}  // namespace views
