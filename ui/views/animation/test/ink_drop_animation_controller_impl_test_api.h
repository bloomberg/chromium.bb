// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_CONTROLLER_IMPL_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_CONTROLLER_IMPL_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropAnimationControllerImpl;
class InkDropHover;

namespace test {

// Test API to provide internal access to an InkDropAnimationControllerImpl
// instance. This can also be used to control the InkDropAnimation and
// InkDropHover animations via the ui::test::MultiLayerAnimatorTestController
// API.
class InkDropAnimationControllerImplTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit InkDropAnimationControllerImplTestApi(
      InkDropAnimationControllerImpl* ink_drop_controller_);
  ~InkDropAnimationControllerImplTestApi() override;

  // Wrappers to InkDropAnimationControllerImpl internals:
  const InkDropHover* hover() const;
  bool IsHoverFadingInOrVisible() const;

 protected:
  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  // The InkDropedAnimation to provide internal access to.
  InkDropAnimationControllerImpl* ink_drop_controller_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerImplTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_CONTROLLER_IMPL_TEST_API_H_
