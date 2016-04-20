// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_ANIMATION_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_ANIMATION_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_animation.h"
#include "ui/views/animation/test/ink_drop_animation_test_api.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
namespace test {

// Test API to provide internal access to a FloodFillInkDropAnimation.
class FloodFillInkDropAnimationTestApi : public InkDropAnimationTestApi {
 public:
  explicit FloodFillInkDropAnimationTestApi(
      FloodFillInkDropAnimation* ink_drop_animation);
  ~FloodFillInkDropAnimationTestApi() override;

  // Wrapper functions to the wrapped InkDropAnimation:
  gfx::Transform CalculateTransform(float target_radius) const;

  // InkDropAnimationTestApi:
  float GetCurrentOpacity() const override;

 protected:
  // InkDropAnimationTestApi:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  FloodFillInkDropAnimation* ink_drop_animation() {
    return static_cast<const FloodFillInkDropAnimationTestApi*>(this)
        ->ink_drop_animation();
  }

  FloodFillInkDropAnimation* ink_drop_animation() const {
    return static_cast<FloodFillInkDropAnimation*>(
        InkDropAnimationTestApi::ink_drop_animation());
  }

  DISALLOW_COPY_AND_ASSIGN(FloodFillInkDropAnimationTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_ANIMATION_TEST_API_H_
