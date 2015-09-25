// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_

#include "ui/gfx/geometry/size.h"

namespace views {
class InkDropAnimation;

namespace test {

// Test API to provide internal access to an InkDropAnimation.
class InkDropAnimationTestApi {
 public:
  typedef InkDropAnimation::InkDropTransforms InkDropTransforms;
  typedef InkDropAnimation::PaintedShape PaintedShape;

  explicit InkDropAnimationTestApi(InkDropAnimation* ink_drop_animation);
  ~InkDropAnimationTestApi();

  // Wrapper functions the wrapped InkDropedAnimation:
  void CalculateCircleTransforms(const gfx::Size& size,
                                 InkDropTransforms* transforms_out) const;
  void CalculateRectTransforms(const gfx::Size& size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

 private:
  // The InkDropAnimation to provide internal access to.
  InkDropAnimation* ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_ANIMATION_TEST_API_H_
