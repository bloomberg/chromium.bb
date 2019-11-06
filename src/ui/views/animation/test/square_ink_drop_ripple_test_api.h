// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_SQUARE_INK_DROP_RIPPLE_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_SQUARE_INK_DROP_RIPPLE_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_ripple_test_api.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
namespace test {

// Test API to provide internal access to a SquareInkDropRipple.
class SquareInkDropRippleTestApi : public InkDropRippleTestApi {
 public:
  // Make the private typedefs accessible.
  using InkDropTransforms = SquareInkDropRipple::InkDropTransforms;
  using PaintedShape = SquareInkDropRipple::PaintedShape;

  explicit SquareInkDropRippleTestApi(SquareInkDropRipple* ink_drop_ripple);
  ~SquareInkDropRippleTestApi() override;

  // Wrapper functions to the wrapped InkDropRipple:
  void CalculateCircleTransforms(const gfx::Size& size,
                                 InkDropTransforms* transforms_out) const;
  void CalculateRectTransforms(const gfx::Size& size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

  // InkDropRippleTestApi:
  float GetCurrentOpacity() const override;

 protected:
  // InkDropRippleTestApi:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  SquareInkDropRipple* ink_drop_ripple() {
    return static_cast<const SquareInkDropRippleTestApi*>(this)
        ->ink_drop_ripple();
  }

  SquareInkDropRipple* ink_drop_ripple() const {
    return static_cast<SquareInkDropRipple*>(
        InkDropRippleTestApi::ink_drop_ripple());
  }

  DISALLOW_COPY_AND_ASSIGN(SquareInkDropRippleTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_SQUARE_INK_DROP_RIPPLE_TEST_API_H_
