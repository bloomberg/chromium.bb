// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_hover_test_api.h"

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/animation/ink_drop_hover.h"

namespace views {
namespace test {

InkDropHoverTestApi::InkDropHoverTestApi(InkDropHover* ink_drop_hover)
    : ui::test::MultiLayerAnimatorTestController(this),
      ink_drop_hover_(ink_drop_hover) {}

InkDropHoverTestApi::~InkDropHoverTestApi() {}

std::vector<ui::LayerAnimator*> InkDropHoverTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators;
  animators.push_back(ink_drop_hover()->layer_->GetAnimator());
  return animators;
}

}  // namespace test
}  // namespace views
