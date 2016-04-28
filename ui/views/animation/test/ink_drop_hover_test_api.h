// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_HOVER_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_HOVER_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropHover;

namespace test {

// Test API to provide internal access to an InkDropHover instance. This can
// also be used to control the animations via the
// ui::test::MultiLayerAnimatorTestController API.
class InkDropHoverTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit InkDropHoverTestApi(InkDropHover* ink_drop_hover);
  ~InkDropHoverTestApi() override;

  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 protected:
  InkDropHover* ink_drop_hover() {
    return static_cast<const InkDropHoverTestApi*>(this)->ink_drop_hover();
  }

  InkDropHover* ink_drop_hover() const { return ink_drop_hover_; }

 private:
  // The InkDropedAnimation to provide internal access to.
  InkDropHover* ink_drop_hover_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHoverTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_HOVER_TEST_API_H_
