// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/test/ink_drop_animation_test_api.h"

namespace views {
namespace test {

InkDropAnimationTestApi::InkDropAnimationTestApi(
    InkDropAnimation* ink_drop_animation)
    : ink_drop_animation_(ink_drop_animation) {}

InkDropAnimationTestApi::~InkDropAnimationTestApi() {}

void InkDropAnimationTestApi::CalculateCircleTransforms(
    const gfx::Size& size,
    InkDropTransforms* transforms_out) const {
  ink_drop_animation_->CalculateCircleTransforms(size, transforms_out);
}
void InkDropAnimationTestApi::CalculateRectTransforms(
    const gfx::Size& size,
    float corner_radius,
    InkDropTransforms* transforms_out) const {
  ink_drop_animation_->CalculateRectTransforms(size, corner_radius,
                                               transforms_out);
}

}  // namespace test
}  // namespace views
