// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

TEST(CSSColorInterpolationTypeTest, GetRGBA1) {
  Color color(230, 120, 0, 255);
  EXPECT_EQ(color,
            CSSColorInterpolationType::GetRGBA(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

TEST(CSSColorInterpolationTypeTest, GetRGBA2) {
  Color color(100, 190, 0, 1);
  EXPECT_EQ(color,
            CSSColorInterpolationType::GetRGBA(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

TEST(CSSColorInterpolationTypeTest, GetRGBA3) {
  Color color(35, 140, 10, 10);
  EXPECT_EQ(color,
            CSSColorInterpolationType::GetRGBA(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

}  // namespace blink
