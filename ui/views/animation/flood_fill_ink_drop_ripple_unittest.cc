// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_ripple.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/flood_fill_ink_drop_ripple_test_api.h"

namespace views {
namespace test {

TEST(FloodFillInkDropRippleTest, TransformedCenterPointForIrregularClipBounds) {
  const gfx::Rect clip_bounds(8, 9, 32, 32);
  const gfx::Point requested_center_point(25, 24);

  // |expected_center_point| is in the |clip_bounds| coordinate space.
  const gfx::Point expected_center_point(
      requested_center_point.x() - clip_bounds.x(),
      requested_center_point.y() - clip_bounds.y());

  FloodFillInkDropRipple ripple(clip_bounds, requested_center_point,
                                SK_ColorWHITE);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  gfx::Point actual_center = test_api.GetDrawnCenterPoint();
  test_api.TransformPoint(10, &actual_center);

  EXPECT_EQ(expected_center_point, actual_center);
}

}  // namespace test
}  // namespace views
