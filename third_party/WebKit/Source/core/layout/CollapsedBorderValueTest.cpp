// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/CollapsedBorderValue.h"

#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CollapsedBorderValueTest, Default) {
  CollapsedBorderValue v;
  EXPECT_EQ(0u, v.Width());
  EXPECT_EQ(EBorderStyle::kNone, v.Style());
  EXPECT_FALSE(v.Exists());
  EXPECT_EQ(kBorderPrecedenceOff, v.Precedence());
  EXPECT_FALSE(v.IsVisible());

  EXPECT_TRUE(v.IsSameIgnoringColor(v));
  EXPECT_TRUE(v.VisuallyEquals(v));
  EXPECT_TRUE(v.IsSameIgnoringColor(CollapsedBorderValue()));
  EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue()));
}

TEST(CollapsedBorderValueTest, SolidZeroWidth) {
  auto style = ComputedStyle::Create();
  style->SetBorderLeftWidth(0);
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  CollapsedBorderValue v(style->BorderLeft(), Color(255, 0, 0),
                         kBorderPrecedenceCell);
  EXPECT_TRUE(v.Exists());
  EXPECT_EQ(0u, v.Width());
  EXPECT_FALSE(v.IsTransparent());
  EXPECT_FALSE(v.IsVisible());

  EXPECT_TRUE(v.IsSameIgnoringColor(v));
  EXPECT_TRUE(v.VisuallyEquals(v));
  EXPECT_FALSE(v.IsSameIgnoringColor(CollapsedBorderValue()));
  EXPECT_FALSE(v.IsSameIgnoringColor(
      CollapsedBorderValue(ComputedStyle::Create()->BorderLeft(),
                           Color(0, 255, 0), kBorderPrecedenceCell)));
  EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue()));
}

TEST(CollapsedBorderValueTest, SolidNonZeroWidthTransparent) {
  auto style = ComputedStyle::Create();
  style->SetBorderLeftWidth(5);
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  EXPECT_EQ(style->BorderLeft().Width(), 5);
  CollapsedBorderValue v(style->BorderLeft(), Color(), kBorderPrecedenceCell);
  EXPECT_TRUE(v.Exists());
  EXPECT_EQ(5u, v.Width());
  EXPECT_TRUE(v.IsTransparent());
  EXPECT_FALSE(v.IsVisible());

  EXPECT_TRUE(v.IsSameIgnoringColor(v));
  EXPECT_TRUE(v.VisuallyEquals(v));
  EXPECT_FALSE(v.IsSameIgnoringColor(CollapsedBorderValue()));
  EXPECT_TRUE(v.IsSameIgnoringColor(CollapsedBorderValue(
      style->BorderLeft(), Color(0, 255, 0), kBorderPrecedenceCell)));
  EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue()));
}

TEST(CollapsedBorderValueTest, None) {
  auto style = ComputedStyle::Create();
  style->SetBorderLeftWidth(5);
  style->SetBorderLeftStyle(EBorderStyle::kNone);
  EXPECT_EQ(style->BorderLeft().Width(), 5);
  CollapsedBorderValue v(style->BorderLeft(), Color(255, 0, 0),
                         kBorderPrecedenceCell);
  EXPECT_TRUE(v.Exists());
  EXPECT_EQ(0u, v.Width());
  EXPECT_FALSE(v.IsTransparent());
  EXPECT_FALSE(v.IsVisible());

  EXPECT_TRUE(v.IsSameIgnoringColor(v));
  EXPECT_TRUE(v.VisuallyEquals(v));
  EXPECT_FALSE(v.IsSameIgnoringColor(CollapsedBorderValue()));
  EXPECT_TRUE(v.IsSameIgnoringColor(
      CollapsedBorderValue(ComputedStyle::Create()->BorderLeft(),
                           Color(0, 255, 0), kBorderPrecedenceCell)));
  EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue()));
}

TEST(CollapsedBorderValueTest, Hidden) {
  auto style = ComputedStyle::Create();
  style->SetBorderLeftWidth(5);
  style->SetBorderLeftStyle(EBorderStyle::kHidden);
  EXPECT_EQ(style->BorderLeft().Width(), 5);
  CollapsedBorderValue v(style->BorderLeft(), Color(255, 0, 0),
                         kBorderPrecedenceCell);
  EXPECT_TRUE(v.Exists());
  EXPECT_EQ(0u, v.Width());
  EXPECT_FALSE(v.IsTransparent());
  EXPECT_FALSE(v.IsVisible());

  EXPECT_TRUE(v.IsSameIgnoringColor(v));
  EXPECT_TRUE(v.VisuallyEquals(v));
  EXPECT_FALSE(v.IsSameIgnoringColor(CollapsedBorderValue()));
  EXPECT_FALSE(v.IsSameIgnoringColor(
      CollapsedBorderValue(ComputedStyle::Create()->BorderLeft(),
                           Color(0, 255, 0), kBorderPrecedenceCell)));
  EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue()));
}

TEST(CollapsedBorderValueTest, SolidNonZeroWidthNonTransparent) {
  auto style = ComputedStyle::Create();
  style->SetBorderLeftWidth(5);
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  EXPECT_EQ(style->BorderLeft().Width(), 5);
  CollapsedBorderValue v(style->BorderLeft(), Color(255, 0, 0),
                         kBorderPrecedenceCell);
  EXPECT_TRUE(v.Exists());
  EXPECT_EQ(5u, v.Width());
  EXPECT_FALSE(v.IsTransparent());
  EXPECT_TRUE(v.IsVisible());

  EXPECT_TRUE(v.IsSameIgnoringColor(v));
  EXPECT_TRUE(v.VisuallyEquals(v));
  EXPECT_FALSE(v.IsSameIgnoringColor(CollapsedBorderValue()));
  EXPECT_FALSE(v.VisuallyEquals(CollapsedBorderValue()));

  EXPECT_TRUE(v.IsSameIgnoringColor(CollapsedBorderValue(
      style->BorderLeft(), Color(0, 255, 0), kBorderPrecedenceCell)));
  EXPECT_FALSE(v.VisuallyEquals(CollapsedBorderValue(
      style->BorderLeft(), Color(0, 255, 0), kBorderPrecedenceCell)));

  EXPECT_FALSE(v.IsSameIgnoringColor(CollapsedBorderValue(
      style->BorderLeft(), Color(255, 0, 0), kBorderPrecedenceTable)));
  EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue(
      style->BorderLeft(), Color(255, 0, 0), kBorderPrecedenceTable)));
}

}  // namespace blink
