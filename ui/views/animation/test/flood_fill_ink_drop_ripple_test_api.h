// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_RIPPLE_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_RIPPLE_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_ripple_test_api.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
namespace test {

// Test API to provide internal access to a FloodFillInkDropRipple.
class FloodFillInkDropRippleTestApi : public InkDropRippleTestApi {
 public:
  explicit FloodFillInkDropRippleTestApi(
      FloodFillInkDropRipple* ink_drop_ripple);
  ~FloodFillInkDropRippleTestApi() override;

  // Wrapper functions to the wrapped InkDropRipple:
  gfx::Transform CalculateTransform(float target_radius) const;

  // InkDropRippleTestApi:
  float GetCurrentOpacity() const override;

 protected:
  // InkDropRippleTestApi:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  FloodFillInkDropRipple* ink_drop_ripple() {
    return static_cast<const FloodFillInkDropRippleTestApi*>(this)
        ->ink_drop_ripple();
  }

  FloodFillInkDropRipple* ink_drop_ripple() const {
    return static_cast<FloodFillInkDropRipple*>(
        InkDropRippleTestApi::ink_drop_ripple());
  }

  DISALLOW_COPY_AND_ASSIGN(FloodFillInkDropRippleTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_RIPPLE_TEST_API_H_
