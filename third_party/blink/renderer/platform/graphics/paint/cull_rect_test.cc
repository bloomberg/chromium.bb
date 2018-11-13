// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

using CullRectTest = testing::Test;

TEST_F(CullRectTest, IntersectsIntRect) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));

  EXPECT_TRUE(cull_rect.Intersects(IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(51, 51, 1, 1)));
}

TEST_F(CullRectTest, IntersectsLayoutRect) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));

  EXPECT_TRUE(cull_rect.Intersects(LayoutRect(0, 0, 1, 1)));
  EXPECT_TRUE(cull_rect.Intersects(LayoutRect(
      LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1))));
}

TEST_F(CullRectTest, IntersectsTransformed) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));
  AffineTransform transform;
  transform.Translate(-2, -2);

  EXPECT_TRUE(
      cull_rect.IntersectsTransformed(transform, FloatRect(51, 51, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(52, 52, 1, 1)));
}

TEST_F(CullRectTest, ApplyTransform) {
  CullRect cull_rect(IntRect(1, 1, 50, 50));
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(1, 1));
  cull_rect.ApplyTransform(transform.get());

  EXPECT_EQ(IntRect(0, 0, 50, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ApplyScrollTranslation) {
  ScopedSlimmingPaintV2ForTest spv2(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation = CreateScrollTranslation(t0(), 10, 15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  cull_rect.ApplyTransform(scroll_translation.get());

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (10, -5, 30, 50)
  // Expanded: (-3990, -4005, 8030, 8050)
  EXPECT_EQ(IntRect(-3990, -4005, 8030, 8050), cull_rect.Rect());
}

TEST_F(CullRectTest, IntersectsVerticalRange) {
  CullRect cull_rect(IntRect(0, 0, 50, 100));

  EXPECT_TRUE(cull_rect.IntersectsVerticalRange(LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(
      cull_rect.IntersectsVerticalRange(LayoutUnit(100), LayoutUnit(101)));
}

TEST_F(CullRectTest, IntersectsHorizontalRange) {
  CullRect cull_rect(IntRect(0, 0, 50, 100));

  EXPECT_TRUE(cull_rect.IntersectsHorizontalRange(LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(
      cull_rect.IntersectsHorizontalRange(LayoutUnit(50), LayoutUnit(51)));
}

}  // namespace blink
