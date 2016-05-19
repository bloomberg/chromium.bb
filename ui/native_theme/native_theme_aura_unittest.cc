// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"

namespace ui {
namespace {
void VerifyPoint(SkPoint a, SkPoint b) {
  EXPECT_EQ(a.x(), b.x());
  EXPECT_EQ(a.y(), b.y());
}

void VerifyTriangle(SkPath actualPath, SkPoint p0, SkPoint p1, SkPoint p2) {
  EXPECT_EQ(3, actualPath.countPoints());
  VerifyPoint(p0, actualPath.getPoint(0));
  VerifyPoint(p1, actualPath.getPoint(1));
  VerifyPoint(p2, actualPath.getPoint(2));
}
}

TEST(NativeThemeAuraTest, VerticalArrows) {
  NativeThemeAura* theme = NativeThemeAura::instance();
  SkPath path;

  // Up arrow, sized for 1x.
  path = theme->PathForArrow(gfx::Rect(100, 200, 17, 17),
                             NativeTheme::kScrollbarUpArrow);
  VerifyTriangle(path, SkPoint::Make(105, 211), SkPoint::Make(112, 211),
                 SkPoint::Make(108.5, 207));

  // 1.25x, should be larger.
  path = theme->PathForArrow(gfx::Rect(50, 70, 21, 21),
                             NativeTheme::kScrollbarUpArrow);
  VerifyTriangle(path, SkPoint::Make(56, 84), SkPoint::Make(65, 84),
                 SkPoint::Make(60.5, 79));

  // Down arrow is just a flipped up arrow.
  path = theme->PathForArrow(gfx::Rect(20, 80, 17, 17),
                             NativeTheme::kScrollbarDownArrow);
  VerifyTriangle(path, SkPoint::Make(25, 86), SkPoint::Make(32, 86),
                 SkPoint::Make(28.5, 90));
}

TEST(NativeThemeAuraTest, HorizontalArrows) {
  NativeThemeAura* theme = NativeThemeAura::instance();
  SkPath path;

  // Right arrow, sized for 1x.
  path = theme->PathForArrow(gfx::Rect(100, 200, 17, 17),
                             NativeTheme::kScrollbarRightArrow);
  VerifyTriangle(path, SkPoint::Make(107, 205), SkPoint::Make(107, 212),
                 SkPoint::Make(111, 208.5));

  // Button size for 1.25x, should be larger.
  path = theme->PathForArrow(gfx::Rect(50, 70, 21, 21),
                             NativeTheme::kScrollbarRightArrow);
  VerifyTriangle(path, SkPoint::Make(58, 76), SkPoint::Make(58, 85),
                 SkPoint::Make(63, 80.5));

  // Left arrow is just a flipped right arrow.
  path = theme->PathForArrow(gfx::Rect(20, 80, 17, 17),
                             NativeTheme::kScrollbarLeftArrow);
  VerifyTriangle(path, SkPoint::Make(30, 85), SkPoint::Make(30, 92),
                 SkPoint::Make(26, 88.5));
}

}  // namespace ui
