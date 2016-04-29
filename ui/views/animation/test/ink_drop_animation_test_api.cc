// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_animation_test_api.h"

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
