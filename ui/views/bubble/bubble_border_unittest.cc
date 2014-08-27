// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "ui/views/test/views_test_base.h"

namespace views {

typedef views::ViewsTestBase BubbleBorderTest;

TEST_F(BubbleBorderTest, GetMirroredArrow) {
  // Horizontal mirroring.
  EXPECT_EQ(BubbleBorder::TOP_RIGHT,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_LEFT));
  EXPECT_EQ(BubbleBorder::TOP_LEFT,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_RIGHT));

  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_LEFT));
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_EQ(BubbleBorder::RIGHT_TOP,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_TOP));
  EXPECT_EQ(BubbleBorder::LEFT_TOP,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_TOP));

  EXPECT_EQ(BubbleBorder::RIGHT_BOTTOM,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_BOTTOM));
  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_EQ(BubbleBorder::TOP_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_CENTER));
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_CENTER));

  EXPECT_EQ(BubbleBorder::RIGHT_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_CENTER));
  EXPECT_EQ(BubbleBorder::LEFT_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_CENTER));

  EXPECT_EQ(BubbleBorder::NONE,
            BubbleBorder::horizontal_mirror(BubbleBorder::NONE));
  EXPECT_EQ(BubbleBorder::FLOAT,
            BubbleBorder::horizontal_mirror(BubbleBorder::FLOAT));

  // Vertical mirroring.
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_LEFT));
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_RIGHT));

  EXPECT_EQ(BubbleBorder::TOP_LEFT,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_LEFT));
  EXPECT_EQ(BubbleBorder::TOP_RIGHT,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_TOP));
  EXPECT_EQ(BubbleBorder::RIGHT_BOTTOM,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_TOP));

  EXPECT_EQ(BubbleBorder::LEFT_TOP,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_BOTTOM));
  EXPECT_EQ(BubbleBorder::RIGHT_TOP,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_CENTER));
  EXPECT_EQ(BubbleBorder::TOP_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_CENTER));

  EXPECT_EQ(BubbleBorder::LEFT_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_CENTER));
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_CENTER));

  EXPECT_EQ(BubbleBorder::NONE,
            BubbleBorder::vertical_mirror(BubbleBorder::NONE));
  EXPECT_EQ(BubbleBorder::FLOAT,
            BubbleBorder::vertical_mirror(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, HasArrow) {
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_LEFT));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_TOP));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_TOP));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_BOTTOM));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_CENTER));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::has_arrow(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::has_arrow(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnLeft) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_TOP));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnTop) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_TOP));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnHorizontal) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowAtCenter) {
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_CENTER));
  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, GetSizeForContentsSizeTest) {
  views::BubbleBorder border(BubbleBorder::NONE,
                             BubbleBorder::NO_SHADOW,
                             SK_ColorWHITE);

  const views::internal::BorderImages* kImages = border.GetImagesForTest();

  // kSmallSize is smaller than the minimum allowable size and does not
  // contribute to the resulting size.
  const gfx::Size kSmallSize = gfx::Size(1, 2);
  // kMediumSize is larger than the minimum allowable size and contributes to
  // the resulting size.
  const gfx::Size kMediumSize = gfx::Size(50, 60);

  const gfx::Size kSmallHorizArrow(
      2 * kImages->border_thickness + kImages->top_arrow.width(),
      kImages->border_thickness + kImages->arrow_thickness +
          kImages->border_interior_thickness);

  const gfx::Size kSmallVertArrow(kSmallHorizArrow.height(),
                                  kSmallHorizArrow.width());

  const gfx::Size kSmallNoArrow(2 * kImages->border_thickness,
                                2 * kImages->border_thickness);

  const gfx::Size kMediumHorizArrow(
      kMediumSize.width() + 2 * border.GetBorderThickness(),
      kMediumSize.height() + border.GetBorderThickness() +
          kImages->arrow_thickness);

  const gfx::Size kMediumVertArrow(
      kMediumSize.width() + border.GetBorderThickness() +
          kImages->arrow_thickness,
      kMediumSize.height() + 2 * border.GetBorderThickness());

  const gfx::Size kMediumNoArrow(
      kMediumSize.width() + 2 * border.GetBorderThickness(),
      kMediumSize.height() + 2 * border.GetBorderThickness());

  struct TestCase {
    BubbleBorder::Arrow arrow;
    gfx::Size content;
    gfx::Size expected_with_arrow;
    gfx::Size expected_without_arrow;
  };

  TestCase cases[] = {
    // Content size: kSmallSize
    { BubbleBorder::TOP_LEFT, kSmallSize, kSmallHorizArrow, kSmallNoArrow },
    { BubbleBorder::TOP_CENTER, kSmallSize, kSmallHorizArrow, kSmallNoArrow },
    { BubbleBorder::TOP_RIGHT, kSmallSize, kSmallHorizArrow, kSmallNoArrow },
    { BubbleBorder::BOTTOM_LEFT, kSmallSize, kSmallHorizArrow, kSmallNoArrow },
    { BubbleBorder::BOTTOM_CENTER, kSmallSize, kSmallHorizArrow,
      kSmallNoArrow },
    { BubbleBorder::BOTTOM_RIGHT, kSmallSize, kSmallHorizArrow, kSmallNoArrow },
    { BubbleBorder::LEFT_TOP, kSmallSize, kSmallVertArrow, kSmallNoArrow },
    { BubbleBorder::LEFT_CENTER, kSmallSize, kSmallVertArrow, kSmallNoArrow },
    { BubbleBorder::LEFT_BOTTOM, kSmallSize, kSmallVertArrow, kSmallNoArrow },
    { BubbleBorder::RIGHT_TOP, kSmallSize, kSmallVertArrow, kSmallNoArrow },
    { BubbleBorder::RIGHT_CENTER, kSmallSize, kSmallVertArrow, kSmallNoArrow },
    { BubbleBorder::RIGHT_BOTTOM, kSmallSize, kSmallVertArrow, kSmallNoArrow },
    { BubbleBorder::NONE, kSmallSize, kSmallNoArrow, kSmallNoArrow },
    { BubbleBorder::FLOAT, kSmallSize, kSmallNoArrow, kSmallNoArrow },

    // Content size: kMediumSize
    { BubbleBorder::TOP_LEFT, kMediumSize, kMediumHorizArrow, kMediumNoArrow },
    { BubbleBorder::TOP_CENTER, kMediumSize, kMediumHorizArrow,
      kMediumNoArrow },
    { BubbleBorder::TOP_RIGHT, kMediumSize, kMediumHorizArrow, kMediumNoArrow },
    { BubbleBorder::BOTTOM_LEFT, kMediumSize, kMediumHorizArrow,
      kMediumNoArrow },
    { BubbleBorder::BOTTOM_CENTER, kMediumSize, kMediumHorizArrow,
      kMediumNoArrow },
    { BubbleBorder::BOTTOM_RIGHT, kMediumSize, kMediumHorizArrow,
      kMediumNoArrow },
    { BubbleBorder::LEFT_TOP, kMediumSize, kMediumVertArrow, kMediumNoArrow },
    { BubbleBorder::LEFT_CENTER, kMediumSize, kMediumVertArrow,
      kMediumNoArrow },
    { BubbleBorder::LEFT_BOTTOM, kMediumSize, kMediumVertArrow,
      kMediumNoArrow },
    { BubbleBorder::RIGHT_TOP, kMediumSize, kMediumVertArrow, kMediumNoArrow },
    { BubbleBorder::RIGHT_CENTER, kMediumSize, kMediumVertArrow,
      kMediumNoArrow },
    { BubbleBorder::RIGHT_BOTTOM, kMediumSize, kMediumVertArrow,
      kMediumNoArrow },
    { BubbleBorder::NONE, kMediumSize, kMediumNoArrow, kMediumNoArrow },
    { BubbleBorder::FLOAT, kMediumSize, kMediumNoArrow, kMediumNoArrow }
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i=%d arrow=%d",
        static_cast<int>(i), cases[i].arrow));

    border.set_arrow(cases[i].arrow);

    border.set_paint_arrow(BubbleBorder::PAINT_NORMAL);
    EXPECT_EQ(cases[i].expected_with_arrow,
              border.GetSizeForContentsSize(cases[i].content));

    border.set_paint_arrow(BubbleBorder::PAINT_TRANSPARENT);
    EXPECT_EQ(cases[i].expected_with_arrow,
              border.GetSizeForContentsSize(cases[i].content));

    border.set_paint_arrow(BubbleBorder::PAINT_NONE);
    EXPECT_EQ(cases[i].expected_without_arrow,
              border.GetSizeForContentsSize(cases[i].content));
  }
}

TEST_F(BubbleBorderTest, GetBoundsOriginTest) {
  views::BubbleBorder border(BubbleBorder::TOP_LEFT,
                             BubbleBorder::NO_SHADOW,
                             SK_ColorWHITE);

  const gfx::Rect kAnchor(100, 100, 20, 30);
  const gfx::Size kContentSize(50, 60);

  const views::internal::BorderImages* kImages = border.GetImagesForTest();

  border.set_arrow(BubbleBorder::TOP_LEFT);
  const gfx::Size kTotalSizeWithHorizArrow =
      border.GetSizeForContentsSize(kContentSize);

  border.set_arrow(BubbleBorder::RIGHT_BOTTOM);
  const gfx::Size kTotalSizeWithVertArrow =
      border.GetSizeForContentsSize(kContentSize);

  border.set_arrow(BubbleBorder::NONE);
  const gfx::Size kTotalSizeWithNoArrow =
      border.GetSizeForContentsSize(kContentSize);

  const int kBorderThickness = border.GetBorderThickness();

  const int kArrowOffsetForHorizCenter = kTotalSizeWithHorizArrow.width() / 2;
  const int kArrowOffsetForVertCenter = kTotalSizeWithVertArrow.height() / 2;
  const int kArrowOffsetForNotCenter =
      kImages->border_thickness + (kImages->top_arrow.width() / 2);

  const int kArrowSize =
      kImages->arrow_interior_thickness + BubbleBorder::kStroke -
          kImages->arrow_thickness;

  const int kTopHorizArrowY = kAnchor.y() + kAnchor.height() + kArrowSize;
  const int kBottomHorizArrowY =
      kAnchor.y() - kArrowSize - kTotalSizeWithHorizArrow.height();

  const int kLeftVertArrowX = kAnchor.x() + kAnchor.width() + kArrowSize;
  const int kRightVertArrowX =
      kAnchor.x() - kArrowSize - kTotalSizeWithVertArrow.width();

  struct TestCase {
    BubbleBorder::Arrow arrow;
    BubbleBorder::BubbleAlignment alignment;
    int expected_x;
    int expected_y;
  };

  TestCase cases[] = {
    // Horizontal arrow tests.
    { BubbleBorder::TOP_LEFT, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kAnchor.CenterPoint().x() - kArrowOffsetForNotCenter, kTopHorizArrowY },
    { BubbleBorder::TOP_LEFT, BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE,
      kAnchor.x() + BubbleBorder::kStroke - kBorderThickness, kTopHorizArrowY },
    { BubbleBorder::TOP_CENTER, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kAnchor.CenterPoint().x() - kArrowOffsetForHorizCenter, kTopHorizArrowY },
    { BubbleBorder::BOTTOM_RIGHT, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kAnchor.CenterPoint().x() + kArrowOffsetForNotCenter -
          kTotalSizeWithHorizArrow.width(), kBottomHorizArrowY },
    { BubbleBorder::BOTTOM_RIGHT, BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE,
      kAnchor.x() + kAnchor.width() - kTotalSizeWithHorizArrow.width() +
          kBorderThickness - BubbleBorder::kStroke, kBottomHorizArrowY },

    // Vertical arrow tests.
    { BubbleBorder::LEFT_TOP, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kLeftVertArrowX, kAnchor.CenterPoint().y() - kArrowOffsetForNotCenter },
    { BubbleBorder::LEFT_TOP, BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE,
      kLeftVertArrowX, kAnchor.y() + BubbleBorder::kStroke - kBorderThickness },
    { BubbleBorder::LEFT_CENTER, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kLeftVertArrowX, kAnchor.CenterPoint().y() - kArrowOffsetForVertCenter },
    { BubbleBorder::RIGHT_BOTTOM, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kRightVertArrowX, kAnchor.CenterPoint().y() + kArrowOffsetForNotCenter -
          kTotalSizeWithVertArrow.height() },
    { BubbleBorder::RIGHT_BOTTOM, BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE,
      kRightVertArrowX, kAnchor.y() + kAnchor.height() -
          kTotalSizeWithVertArrow.height() + kBorderThickness -
          BubbleBorder::kStroke },

    // No arrow tests.
    { BubbleBorder::NONE, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kAnchor.x() + (kAnchor.width() - kTotalSizeWithNoArrow.width()) / 2,
      kAnchor.y() + kAnchor.height() },
    { BubbleBorder::FLOAT, BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
      kAnchor.x() + (kAnchor.width() - kTotalSizeWithNoArrow.width()) / 2,
      kAnchor.y() + (kAnchor.height() - kTotalSizeWithNoArrow.height()) / 2 },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i=%d arrow=%d alignment=%d",
        static_cast<int>(i), cases[i].arrow, cases[i].alignment));
    border.set_arrow(cases[i].arrow);
    border.set_alignment(cases[i].alignment);

    gfx::Point origin = border.GetBounds(kAnchor, kContentSize).origin();
    EXPECT_EQ(cases[i].expected_x, origin.x());
    EXPECT_EQ(cases[i].expected_y, origin.y());
  }
}

}  // namespace views
