// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_IMPL_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_IMPL_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropImpl;
class InkDropHighlight;

namespace test {

// Test API to provide internal access to an InkDropImpl instance. This can also
// be used to control the InkDropRipple and InkDropHighlight animations via the
// ui::test::MultiLayerAnimatorTestController API.
class InkDropImplTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit InkDropImplTestApi(InkDropImpl* ink_drop);
  ~InkDropImplTestApi() override;

  // Wrappers to InkDropImpl internals:
  const InkDropHighlight* highlight() const;
  bool IsHighlightFadingInOrVisible() const;

 protected:
  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  // The InkDrop to provide internal access to.
  InkDropImpl* ink_drop_;

  DISALLOW_COPY_AND_ASSIGN(InkDropImplTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_IMPL_TEST_API_H_
