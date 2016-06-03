// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_impl_test_api.h"

#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/animation/test/ink_drop_ripple_test_api.h"

namespace views {
namespace test {

InkDropImplTestApi::InkDropImplTestApi(InkDropImpl* ink_drop)
    : ui::test::MultiLayerAnimatorTestController(this), ink_drop_(ink_drop) {}

InkDropImplTestApi::~InkDropImplTestApi() {}

const InkDropHighlight* InkDropImplTestApi::highlight() const {
  return ink_drop_->highlight_.get();
}

bool InkDropImplTestApi::IsHighlightFadingInOrVisible() const {
  return ink_drop_->IsHighlightFadingInOrVisible();
}

std::vector<ui::LayerAnimator*> InkDropImplTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators;

  if (ink_drop_->highlight_) {
    InkDropHighlightTestApi* ink_drop_highlight_test_api =
        ink_drop_->highlight_->GetTestApi();
    std::vector<ui::LayerAnimator*> ink_drop_highlight_animators =
        ink_drop_highlight_test_api->GetLayerAnimators();
    animators.insert(animators.end(), ink_drop_highlight_animators.begin(),
                     ink_drop_highlight_animators.end());
  }

  if (ink_drop_->ink_drop_ripple_) {
    InkDropRippleTestApi* ink_drop_ripple_test_api =
        ink_drop_->ink_drop_ripple_->GetTestApi();
    std::vector<ui::LayerAnimator*> ink_drop_ripple_animators =
        ink_drop_ripple_test_api->GetLayerAnimators();
    animators.insert(animators.end(), ink_drop_ripple_animators.begin(),
                     ink_drop_ripple_animators.end());
  }

  return animators;
}

}  // namespace test
}  // namespace views
