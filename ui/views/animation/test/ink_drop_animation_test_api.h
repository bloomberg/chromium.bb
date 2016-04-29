// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropAnimation;

namespace test {

// Test API to provide internal access to an InkDropAnimation instance. This can
// also be used to control the animations via the
// ui::test::MultiLayerAnimatorTestController API.
class InkDropAnimationTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit InkDropAnimationTestApi(InkDropAnimation* ink_drop_animation);
  ~InkDropAnimationTestApi() override;

  // Gets the opacity of the ink drop.
  virtual float GetCurrentOpacity() const = 0;

  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 protected:
  InkDropAnimation* ink_drop_animation() {
    return static_cast<const InkDropAnimationTestApi*>(this)
        ->ink_drop_animation();
  }

  InkDropAnimation* ink_drop_animation() const { return ink_drop_animation_; }

 private:
  // The InkDropedAnimation to provide internal access to.
  InkDropAnimation* ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
