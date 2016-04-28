// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_animation_controller_impl_test_api.h"

#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/ink_drop_animation_controller_impl.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/test/ink_drop_animation_test_api.h"
#include "ui/views/animation/test/ink_drop_hover_test_api.h"

namespace views {
namespace test {

InkDropAnimationControllerImplTestApi::InkDropAnimationControllerImplTestApi(
    InkDropAnimationControllerImpl* ink_drop_controller)
    : ui::test::MultiLayerAnimatorTestController(this),
      ink_drop_controller_(ink_drop_controller) {}

InkDropAnimationControllerImplTestApi::
    ~InkDropAnimationControllerImplTestApi() {}

const InkDropHover* InkDropAnimationControllerImplTestApi::hover() const {
  return ink_drop_controller_->hover_.get();
}

bool InkDropAnimationControllerImplTestApi::IsHoverFadingInOrVisible() const {
  return ink_drop_controller_->IsHoverFadingInOrVisible();
}

std::vector<ui::LayerAnimator*>
InkDropAnimationControllerImplTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators;

  if (ink_drop_controller_->hover_) {
    InkDropHoverTestApi* ink_drop_hover_test_api =
        ink_drop_controller_->hover_->GetTestApi();
    std::vector<ui::LayerAnimator*> ink_drop_hover_animators =
        ink_drop_hover_test_api->GetLayerAnimators();
    animators.insert(animators.end(), ink_drop_hover_animators.begin(),
                     ink_drop_hover_animators.end());
  }

  if (ink_drop_controller_->ink_drop_animation_) {
    InkDropAnimationTestApi* ink_drop_animation_test_api =
        ink_drop_controller_->ink_drop_animation_->GetTestApi();
    std::vector<ui::LayerAnimator*> ink_drop_animation_animators =
        ink_drop_animation_test_api->GetLayerAnimators();
    animators.insert(animators.end(), ink_drop_animation_animators.begin(),
                     ink_drop_animation_animators.end());
  }

  return animators;
}

}  // namespace test
}  // namespace views
