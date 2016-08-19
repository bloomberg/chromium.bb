// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_margin_strut.h"
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

  static NGConstraintSpace constructConstraintSpace(int inline_size,
                                                    int block_size) {
    NGLogicalSize container_size;
    container_size.inlineSize = LayoutUnit(inline_size);
    container_size.blockSize = LayoutUnit(block_size);
    return NGConstraintSpace(container_size);
  }

  LayoutUnit resolveInlineLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize) {
    NGConstraintSpace constraintSpace = constructConstraintSpace(200, 300);
    return ::blink::resolveInlineLength(constraintSpace, length, type);
  }

  LayoutUnit resolveBlockLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize,
      LayoutUnit contentSize = LayoutUnit()) {
    NGConstraintSpace constraintSpace = constructConstraintSpace(200, 300);
    return ::blink::resolveBlockLength(constraintSpace, length, contentSize,
                                       type);
  }

  LayoutUnit computeInlineSizeForFragment(
      const NGConstraintSpace constraintSpace = constructConstraintSpace(200,
                                                                         300)) {
    return ::blink::computeInlineSizeForFragment(constraintSpace, *style_);
  }

  LayoutUnit computeBlockSizeForFragment(
      const NGConstraintSpace constraintSpace = constructConstraintSpace(200,
                                                                         300),
      LayoutUnit contentSize = LayoutUnit()) {
    return ::blink::computeBlockSizeForFragment(constraintSpace, *style_,
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

  NGConstraintSpace constraintSpace = constructConstraintSpace(120, 120);
  constraintSpace.setFixedSize(true, true);
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
            computeBlockSizeForFragment(constructConstraintSpace(200, 300),
                                        LayoutUnit(120)));

  style_->setLogicalHeight(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(300), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(CalculationValue::create(
      PixelsAndPercent(100, -10), ValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(70), computeBlockSizeForFragment());

  NGConstraintSpace constraintSpace = constructConstraintSpace(200, 200);
  constraintSpace.setFixedSize(true, true);
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

TEST_F(NGLengthUtilsTest, testMargins) {
  style_->setMarginTop(Length(10, Percent));
  style_->setMarginRight(Length(52, Fixed));
  style_->setMarginBottom(Length(Auto));
  style_->setMarginLeft(Length(11, Percent));

  NGConstraintSpace constraintSpace(constructConstraintSpace(200, 300));

  NGBoxMargins margins = computeMargins(constraintSpace, *style_);

  EXPECT_EQ(LayoutUnit(20), margins.blockStart);
  EXPECT_EQ(LayoutUnit(52), margins.inlineEnd);
  EXPECT_EQ(LayoutUnit(), margins.blockEnd);
  EXPECT_EQ(LayoutUnit(22), margins.inlineStart);
}

}  // namespace
}  // namespace blink
