// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "platform/CalculationValue.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

class NGLengthUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  static NGConstraintSpace* ConstructConstraintSpace(int inline_size,
                                                     int block_size,
                                                     bool fixed_inline = false,
                                                     bool fixed_block = false) {
    NGConstraintSpace* derived_constraint_space = new NGConstraintSpace(
        HorizontalTopBottom,
        NGLogicalSize(LayoutUnit(inline_size), LayoutUnit(block_size)));
    derived_constraint_space->SetOverflowTriggersScrollbar(false, false);
    derived_constraint_space->SetFixedSize(fixed_inline, fixed_block);
    return derived_constraint_space;
  }

  LayoutUnit resolveInlineLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize) {
    NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200, 300);
    return ::blink::resolveInlineLength(*constraintSpace, length, type);
  }

  LayoutUnit resolveBlockLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize,
      LayoutUnit contentSize = LayoutUnit()) {
    NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200, 300);
    return ::blink::resolveBlockLength(*constraintSpace, length, contentSize,
                                       type);
  }

  LayoutUnit computeInlineSizeForFragment(
      const NGConstraintSpace* constraintSpace =
          ConstructConstraintSpace(200, 300)) {
    return ::blink::computeInlineSizeForFragment(*constraintSpace, *style_);
  }

  LayoutUnit computeBlockSizeForFragment(
      const NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200,
                                                                          300),
      LayoutUnit contentSize = LayoutUnit()) {
    return ::blink::computeBlockSizeForFragment(*constraintSpace, *style_,
                                                contentSize);
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGLengthUtilsTest, testResolveInlineLength) {
  EXPECT_EQ(LayoutUnit(60), resolveInlineLength(Length(30, Percent)));
  EXPECT_EQ(LayoutUnit(150), resolveInlineLength(Length(150, Fixed)));
  EXPECT_EQ(LayoutUnit(0),
            resolveInlineLength(Length(Auto), LengthResolveType::MinSize));
  EXPECT_EQ(LayoutUnit(200), resolveInlineLength(Length(Auto)));
  EXPECT_EQ(LayoutUnit(200), resolveInlineLength(Length(FillAvailable)));

  EXPECT_EQ(LayoutUnit(200),
            resolveInlineLength(Length(Auto), LengthResolveType::MaxSize));
  EXPECT_EQ(LayoutUnit(200), resolveInlineLength(Length(FillAvailable),
                                                 LengthResolveType::MaxSize));
}

TEST_F(NGLengthUtilsTest, testResolveBlockLength) {
  EXPECT_EQ(LayoutUnit(90), resolveBlockLength(Length(30, Percent)));
  EXPECT_EQ(LayoutUnit(150), resolveBlockLength(Length(150, Fixed)));
  EXPECT_EQ(LayoutUnit(0), resolveBlockLength(Length(Auto)));
  EXPECT_EQ(LayoutUnit(300), resolveBlockLength(Length(FillAvailable)));

  EXPECT_EQ(LayoutUnit(0),
            resolveBlockLength(Length(Auto), LengthResolveType::ContentSize));
  EXPECT_EQ(LayoutUnit(300),
            resolveBlockLength(Length(FillAvailable),
                               LengthResolveType::ContentSize));
}

TEST_F(NGLengthUtilsTest, testComputeInlineSizeForFragment) {
  style_->setLogicalWidth(Length(30, Percent));
  EXPECT_EQ(LayoutUnit(60), computeInlineSizeForFragment());

  style_->setLogicalWidth(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(150), computeInlineSizeForFragment());

  style_->setLogicalWidth(Length(Auto));
  EXPECT_EQ(LayoutUnit(200), computeInlineSizeForFragment());

  style_->setLogicalWidth(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(200), computeInlineSizeForFragment());

  style_->setLogicalWidth(Length(CalculationValue::create(
      PixelsAndPercent(100, -10), ValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(80), computeInlineSizeForFragment());

  NGConstraintSpace* constraintSpace =
      ConstructConstraintSpace(120, 120, true, true);
  style_->setLogicalWidth(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(120), computeInlineSizeForFragment(constraintSpace));

  style_->setLogicalWidth(Length(200, Fixed));
  style_->setMaxWidth(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(160), computeInlineSizeForFragment());

  style_->setLogicalWidth(Length(100, Fixed));
  style_->setMinWidth(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(160), computeInlineSizeForFragment());

  // TODO(layout-ng): test {min,max}-content on max-width.
}

TEST_F(NGLengthUtilsTest, testComputeBlockSizeForFragment) {
  style_->setLogicalHeight(Length(30, Percent));
  EXPECT_EQ(LayoutUnit(90), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(150), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(Auto));
  EXPECT_EQ(LayoutUnit(0), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(Auto));
  EXPECT_EQ(LayoutUnit(120),
            computeBlockSizeForFragment(ConstructConstraintSpace(200, 300),
                                        LayoutUnit(120)));

  style_->setLogicalHeight(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(300), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(CalculationValue::create(
      PixelsAndPercent(100, -10), ValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(70), computeBlockSizeForFragment());

  NGConstraintSpace* constraintSpace =
      ConstructConstraintSpace(200, 200, true, true);
  style_->setLogicalHeight(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(200), computeBlockSizeForFragment(constraintSpace));

  style_->setLogicalHeight(Length(300, Fixed));
  style_->setMaxHeight(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(240), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(100, Fixed));
  style_->setMinHeight(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(240), computeBlockSizeForFragment());

  // TODO(layout-ng): test {min,max}-content on max-height.
}

TEST_F(NGLengthUtilsTest, testIndefinitePercentages) {
  style_->setMinHeight(Length(20, Fixed));
  style_->setHeight(Length(20, Percent));

  EXPECT_EQ(NGSizeIndefinite,
            computeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(-1)));
  EXPECT_EQ(LayoutUnit(20),
            computeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(120),
            computeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(120)));
}

TEST_F(NGLengthUtilsTest, testMargins) {
  style_->setMarginTop(Length(10, Percent));
  style_->setMarginRight(Length(52, Fixed));
  style_->setMarginBottom(Length(Auto));
  style_->setMarginLeft(Length(11, Percent));

  NGConstraintSpace* constraintSpace(ConstructConstraintSpace(200, 300));

  NGBoxStrut margins = computeMargins(*constraintSpace, *style_);

  EXPECT_EQ(LayoutUnit(20), margins.block_start);
  EXPECT_EQ(LayoutUnit(52), margins.inline_end);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(22), margins.inline_start);
}

TEST_F(NGLengthUtilsTest, testBorders) {
  style_->setBorderTopWidth(1);
  style_->setBorderRightWidth(2);
  style_->setBorderBottomWidth(3);
  style_->setBorderLeftWidth(4);
  style_->setBorderTopStyle(BorderStyleSolid);
  style_->setBorderRightStyle(BorderStyleSolid);
  style_->setBorderBottomStyle(BorderStyleSolid);
  style_->setBorderLeftStyle(BorderStyleSolid);
  style_->setWritingMode(LeftToRightWritingMode);

  NGBoxStrut borders = computeBorders(*style_);

  EXPECT_EQ(LayoutUnit(4), borders.block_start);
  EXPECT_EQ(LayoutUnit(3), borders.inline_end);
  EXPECT_EQ(LayoutUnit(2), borders.block_end);
  EXPECT_EQ(LayoutUnit(1), borders.inline_start);
}

TEST_F(NGLengthUtilsTest, testPadding) {
  style_->setPaddingTop(Length(10, Percent));
  style_->setPaddingRight(Length(52, Fixed));
  style_->setPaddingBottom(Length(Auto));
  style_->setPaddingLeft(Length(11, Percent));
  style_->setWritingMode(RightToLeftWritingMode);

  NGConstraintSpace* constraintSpace(ConstructConstraintSpace(200, 300));

  NGBoxStrut padding = computePadding(*constraintSpace, *style_);

  EXPECT_EQ(LayoutUnit(52), padding.block_start);
  EXPECT_EQ(LayoutUnit(), padding.inline_end);
  EXPECT_EQ(LayoutUnit(22), padding.block_end);
  EXPECT_EQ(LayoutUnit(20), padding.inline_start);
}

}  // namespace
}  // namespace blink
