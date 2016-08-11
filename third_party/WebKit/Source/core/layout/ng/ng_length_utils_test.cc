// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
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

  LayoutUnit resolveInlineLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize) {
    NGConstraintSpace constraintSpace(LayoutUnit(200), LayoutUnit(300));
    return ::blink::resolveInlineLength(constraintSpace, length, type);
  }

  LayoutUnit resolveBlockLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize,
      LayoutUnit contentSize = LayoutUnit()) {
    NGConstraintSpace constraintSpace(LayoutUnit(200), LayoutUnit(300));
    return ::blink::resolveBlockLength(constraintSpace, length, contentSize,
                                       type);
  }

  LayoutUnit computeInlineSizeForFragment(
      const NGConstraintSpace& constraintSpace =
          NGConstraintSpace(LayoutUnit(200), LayoutUnit(300))) {
    return ::blink::computeInlineSizeForFragment(constraintSpace, *style_);
  }

  LayoutUnit computeBlockSizeForFragment(
      const NGConstraintSpace& constraintSpace =
          NGConstraintSpace(LayoutUnit(200), LayoutUnit(300)),
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

  NGConstraintSpace constraintSpace(LayoutUnit(120), LayoutUnit(120));
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
            computeBlockSizeForFragment(
                NGConstraintSpace(LayoutUnit(200), LayoutUnit(300)),
                LayoutUnit(120)));

  style_->setLogicalHeight(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(300), computeBlockSizeForFragment());

  style_->setLogicalHeight(Length(CalculationValue::create(
      PixelsAndPercent(100, -10), ValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(70), computeBlockSizeForFragment());

  NGConstraintSpace constraintSpace(LayoutUnit(200), LayoutUnit(200));
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

}  // namespace
}  // namespace blink
