// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_SQUARE_INK_DROP_ANIMATION_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_SQUARE_INK_DROP_ANIMATION_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/square_ink_drop_animation.h"
#include "ui/views/animation/test/ink_drop_animation_test_api.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
namespace test {

// Test API to provide internal access to a SquareInkDropAnimation.
class SquareInkDropAnimationTestApi : public InkDropAnimationTestApi {
 public:
  // Make the private typedefs accessible.
  using InkDropTransforms = SquareInkDropAnimation::InkDropTransforms;
  using PaintedShape = SquareInkDropAnimation::PaintedShape;

  explicit SquareInkDropAnimationTestApi(
      SquareInkDropAnimation* ink_drop_animation);
  ~SquareInkDropAnimationTestApi() override;

  // Wrapper functions to the wrapped InkDropAnimation:
  void CalculateCircleTransforms(const gfx::Size& size,
                                 InkDropTransforms* transforms_out) const;
  void CalculateRectTransforms(const gfx::Size& size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

  // InkDropAnimationTestApi:
  float GetCurrentOpacity() const override;

 protected:
  // InkDropAnimationTestApi:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  SquareInkDropAnimation* ink_drop_animation() {
    return static_cast<const SquareInkDropAnimationTestApi*>(this)
        ->ink_drop_animation();
  }

  SquareInkDropAnimation* ink_drop_animation() const {
    return static_cast<SquareInkDropAnimation*>(
        InkDropAnimationTestApi::ink_drop_animation());
  }

  DISALLOW_COPY_AND_ASSIGN(SquareInkDropAnimationTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_SQUARE_INK_DROP_ANIMATION_TEST_API_H_
