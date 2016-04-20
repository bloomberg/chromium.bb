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
    : ui::test::MultiLayerAnimatorTestController(this),
      ink_drop_animation_(ink_drop_animation) {}

InkDropAnimationTestApi::~InkDropAnimationTestApi() {}

std::vector<ui::LayerAnimator*> InkDropAnimationTestApi::GetLayerAnimators() {
  return std::vector<ui::LayerAnimator*>();
}

}  // namespace test
}  // namespace views
