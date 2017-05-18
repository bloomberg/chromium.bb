// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/CalculationValue.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"
#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class NGLengthUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::Create(); }

  static RefPtr<NGConstraintSpace> ConstructConstraintSpace(
      int inline_size,
      int block_size,
      bool fixed_inline = false,
      bool fixed_block = false) {
    return NGConstraintSpaceBuilder(kHorizontalTopBottom)
        .SetAvailableSize(
            NGLogicalSize(LayoutUnit(inline_size), LayoutUnit(block_size)))
        .SetPercentageResolutionSize(
            NGLogicalSize(LayoutUnit(inline_size), LayoutUnit(block_size)))
        .SetIsFixedSizeInline(fixed_inline)
        .SetIsFixedSizeBlock(fixed_block)
        .ToConstraintSpace(kHorizontalTopBottom);
  }

  LayoutUnit ResolveInlineLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::kContentSize,
      const WTF::Optional<MinMaxContentSize>& sizes = WTF::nullopt) {
    RefPtr<NGConstraintSpace> constraint_space =
        ConstructConstraintSpace(200, 300);
    return ::blink::ResolveInlineLength(*constraint_space, *style_, sizes,
                                        length, type);
  }

  LayoutUnit ResolveBlockLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::kContentSize,
      LayoutUnit content_size = LayoutUnit()) {
    RefPtr<NGConstraintSpace> constraint_space =
        ConstructConstraintSpace(200, 300);
    return ::blink::ResolveBlockLength(*constraint_space, *style_, length,
                                       content_size, type);
  }

  LayoutUnit ComputeInlineSizeForFragment(
      RefPtr<const NGConstraintSpace> constraint_space =
          ConstructConstraintSpace(200, 300),
      const MinMaxContentSize& sizes = MinMaxContentSize()) {
    return ::blink::ComputeInlineSizeForFragment(*constraint_space, *style_,
                                                 sizes);
  }

  LayoutUnit ComputeBlockSizeForFragment(
      RefPtr<const NGConstraintSpace> constraint_space =
          ConstructConstraintSpace(200, 300),
      LayoutUnit content_size = LayoutUnit()) {
    return ::blink::ComputeBlockSizeForFragment(*constraint_space, *style_,
                                                content_size);
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGLengthUtilsTest, testResolveInlineLength) {
  EXPECT_EQ(LayoutUnit(60), ResolveInlineLength(Length(30, kPercent)));
  EXPECT_EQ(LayoutUnit(150), ResolveInlineLength(Length(150, kFixed)));
  EXPECT_EQ(LayoutUnit(0),
            ResolveInlineLength(Length(kAuto), LengthResolveType::kMinSize));
  EXPECT_EQ(LayoutUnit(200), ResolveInlineLength(Length(kAuto)));
  EXPECT_EQ(LayoutUnit(200), ResolveInlineLength(Length(kFillAvailable)));

  EXPECT_EQ(LayoutUnit(200),
            ResolveInlineLength(Length(kAuto), LengthResolveType::kMaxSize));
  EXPECT_EQ(LayoutUnit(200), ResolveInlineLength(Length(kFillAvailable),
                                                 LengthResolveType::kMaxSize));
  MinMaxContentSize sizes;
  sizes.min_content = LayoutUnit(30);
  sizes.max_content = LayoutUnit(40);
  EXPECT_EQ(LayoutUnit(30),
            ResolveInlineLength(Length(kMinContent),
                                LengthResolveType::kContentSize, sizes));
  EXPECT_EQ(LayoutUnit(40),
            ResolveInlineLength(Length(kMaxContent),
                                LengthResolveType::kContentSize, sizes));
  EXPECT_EQ(LayoutUnit(40),
            ResolveInlineLength(Length(kFitContent),
                                LengthResolveType::kContentSize, sizes));
  sizes.max_content = LayoutUnit(800);
  EXPECT_EQ(LayoutUnit(200),
            ResolveInlineLength(Length(kFitContent),
                                LengthResolveType::kContentSize, sizes));
#ifndef NDEBUG
  // This should fail a DCHECK.
  EXPECT_DEATH(ResolveInlineLength(Length(kFitContent)), "Check failed");
#endif
}

TEST_F(NGLengthUtilsTest, testResolveBlockLength) {
  EXPECT_EQ(LayoutUnit(90), ResolveBlockLength(Length(30, kPercent)));
  EXPECT_EQ(LayoutUnit(150), ResolveBlockLength(Length(150, kFixed)));
  EXPECT_EQ(LayoutUnit(0), ResolveBlockLength(Length(kAuto)));
  EXPECT_EQ(LayoutUnit(300), ResolveBlockLength(Length(kFillAvailable)));

  EXPECT_EQ(LayoutUnit(0),
            ResolveBlockLength(Length(kAuto), LengthResolveType::kContentSize));
  EXPECT_EQ(LayoutUnit(300),
            ResolveBlockLength(Length(kFillAvailable),
                               LengthResolveType::kContentSize));
}

TEST_F(NGLengthUtilsTest, testComputeContentContribution) {
  MinMaxContentSize sizes;
  sizes.min_content = LayoutUnit(30);
  sizes.max_content = LayoutUnit(40);

  MinMaxContentSize expected{LayoutUnit(), LayoutUnit()};
  style_->SetLogicalWidth(Length(30, kPercent));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  style_->SetLogicalWidth(Length(kFillAvailable));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(150), LayoutUnit(150)};
  style_->SetLogicalWidth(Length(150, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = sizes;
  style_->SetLogicalWidth(Length(kAuto));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(430), LayoutUnit(440)};
  style_->SetPaddingLeft(Length(400, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(100), LayoutUnit(100)};
  style_->SetPaddingLeft(Length(0, kFixed));
  style_->SetLogicalWidth(Length(CalculationValue::Create(
      PixelsAndPercent(100, -10), kValueRangeNonNegative)));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(30), LayoutUnit(35)};
  style_->SetLogicalWidth(Length(kAuto));
  style_->SetMaxWidth(Length(35, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(80), LayoutUnit(80)};
  style_->SetLogicalWidth(Length(50, kFixed));
  style_->SetMinWidth(Length(80, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(150), LayoutUnit(150)};
  style_ = ComputedStyle::Create();
  style_->SetLogicalWidth(Length(100, kFixed));
  style_->SetPaddingLeft(Length(50, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected = MinMaxContentSize{LayoutUnit(100), LayoutUnit(100)};
  style_->SetBoxSizing(EBoxSizing::kBorderBox);
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  expected = MinMaxContentSize{LayoutUnit(400), LayoutUnit(400)};
  style_->SetPaddingLeft(Length(400, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));

  expected.min_content = expected.max_content =
      sizes.min_content + LayoutUnit(400);
  style_->SetLogicalWidth(Length(kMinContent));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));
  style_->SetLogicalWidth(Length(100, kFixed));
  style_->SetMaxWidth(Length(kMaxContent));
  // Due to padding and box-sizing, width computes to 400px and max-width to
  // 440px, so the result is 400.
  expected = MinMaxContentSize{LayoutUnit(400), LayoutUnit(400)};
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));
  expected = MinMaxContentSize{LayoutUnit(40), LayoutUnit(40)};
  style_->SetPaddingLeft(Length(0, kFixed));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(*style_, sizes));
}

TEST_F(NGLengthUtilsTest, testComputeInlineSizeForFragment) {
  MinMaxContentSize sizes;
  sizes.min_content = LayoutUnit(30);
  sizes.max_content = LayoutUnit(40);

  style_->SetLogicalWidth(Length(30, kPercent));
  EXPECT_EQ(LayoutUnit(60), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(150, kFixed));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(kAuto));
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(kFillAvailable));
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(CalculationValue::Create(
      PixelsAndPercent(100, -10), kValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(80), ComputeInlineSizeForFragment());

  RefPtr<NGConstraintSpace> constraint_space =
      ConstructConstraintSpace(120, 120, true, true);
  style_->SetLogicalWidth(Length(150, kFixed));
  EXPECT_EQ(LayoutUnit(120), ComputeInlineSizeForFragment(constraint_space));

  style_->SetLogicalWidth(Length(200, kFixed));
  style_->SetMaxWidth(Length(80, kPercent));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(100, kFixed));
  style_->SetMinWidth(Length(80, kPercent));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment());

  style_ = ComputedStyle::Create();
  style_->SetMarginRight(Length(20, kFixed));
  EXPECT_EQ(LayoutUnit(180), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(100, kFixed));
  style_->SetPaddingLeft(Length(50, kFixed));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment());

  style_->SetBoxSizing(EBoxSizing::kBorderBox);
  EXPECT_EQ(LayoutUnit(100), ComputeInlineSizeForFragment());

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  style_->SetPaddingLeft(Length(400, kFixed));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment());

  // ...and the same goes for fill-available with a large padding.
  style_->SetLogicalWidth(Length(kFillAvailable));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment());

  constraint_space = ConstructConstraintSpace(120, 140);
  style_->SetLogicalWidth(Length(kMinContent));
  EXPECT_EQ(LayoutUnit(430),
            ComputeInlineSizeForFragment(constraint_space, sizes));
  style_->SetLogicalWidth(Length(100, kFixed));
  style_->SetMaxWidth(Length(kMaxContent));
  // Due to padding and box-sizing, width computes to 400px and max-width to
  // 440px, so the result is 400.
  EXPECT_EQ(LayoutUnit(400),
            ComputeInlineSizeForFragment(constraint_space, sizes));
  style_->SetPaddingLeft(Length(0, kFixed));
  EXPECT_EQ(LayoutUnit(40),
            ComputeInlineSizeForFragment(constraint_space, sizes));
}

TEST_F(NGLengthUtilsTest, testComputeBlockSizeForFragment) {
  style_->SetLogicalHeight(Length(30, kPercent));
  EXPECT_EQ(LayoutUnit(90), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(150, kFixed));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(kAuto));
  EXPECT_EQ(LayoutUnit(0), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(kAuto));
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, 300),
                                        LayoutUnit(120)));

  style_->SetLogicalHeight(Length(kFillAvailable));
  EXPECT_EQ(LayoutUnit(300), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(CalculationValue::Create(
      PixelsAndPercent(100, -10), kValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(70), ComputeBlockSizeForFragment());

  RefPtr<NGConstraintSpace> constraint_space =
      ConstructConstraintSpace(200, 200, true, true);
  style_->SetLogicalHeight(Length(150, kFixed));
  EXPECT_EQ(LayoutUnit(200), ComputeBlockSizeForFragment(constraint_space));

  style_->SetLogicalHeight(Length(300, kFixed));
  style_->SetMaxHeight(Length(80, kPercent));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(100, kFixed));
  style_->SetMinHeight(Length(80, kPercent));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment());

  style_ = ComputedStyle::Create();
  style_->SetMarginTop(Length(20, kFixed));
  style_->SetLogicalHeight(Length(kFillAvailable));
  EXPECT_EQ(LayoutUnit(280), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(100, kFixed));
  style_->SetPaddingBottom(Length(50, kFixed));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment());

  style_->SetBoxSizing(EBoxSizing::kBorderBox);
  EXPECT_EQ(LayoutUnit(100), ComputeBlockSizeForFragment());

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  style_->SetPaddingBottom(Length(400, kFixed));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment());

  // ...and the same goes for fill-available with a large padding.
  style_->SetLogicalHeight(Length(kFillAvailable));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment());

  // TODO(layout-ng): test {min,max}-content on max-height.
}

TEST_F(NGLengthUtilsTest, testIndefinitePercentages) {
  style_->SetMinHeight(Length(20, kFixed));
  style_->SetHeight(Length(20, kPercent));

  EXPECT_EQ(NGSizeIndefinite,
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(-1)));
  EXPECT_EQ(LayoutUnit(20),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(120)));
}

TEST_F(NGLengthUtilsTest, testMargins) {
  style_->SetMarginTop(Length(10, kPercent));
  style_->SetMarginRight(Length(52, kFixed));
  style_->SetMarginBottom(Length(kAuto));
  style_->SetMarginLeft(Length(11, kPercent));

  RefPtr<NGConstraintSpace> constraint_space(
      ConstructConstraintSpace(200, 300));

  NGBoxStrut margins = ComputeMargins(
      *constraint_space, *style_, kHorizontalTopBottom, TextDirection::kLtr);

  EXPECT_EQ(LayoutUnit(20), margins.block_start);
  EXPECT_EQ(LayoutUnit(52), margins.inline_end);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(22), margins.inline_start);
}

TEST_F(NGLengthUtilsTest, testBorders) {
  style_->SetBorderTopWidth(1);
  style_->SetBorderRightWidth(2);
  style_->SetBorderBottomWidth(3);
  style_->SetBorderLeftWidth(4);
  style_->SetBorderTopStyle(EBorderStyle::kSolid);
  style_->SetBorderRightStyle(EBorderStyle::kSolid);
  style_->SetBorderBottomStyle(EBorderStyle::kSolid);
  style_->SetBorderLeftStyle(EBorderStyle::kSolid);
  style_->SetWritingMode(WritingMode::kVerticalLr);

  RefPtr<NGConstraintSpace> constraint_space(
      ConstructConstraintSpace(200, 300));

  NGBoxStrut borders = ComputeBorders(*constraint_space, *style_);

  EXPECT_EQ(LayoutUnit(4), borders.block_start);
  EXPECT_EQ(LayoutUnit(3), borders.inline_end);
  EXPECT_EQ(LayoutUnit(2), borders.block_end);
  EXPECT_EQ(LayoutUnit(1), borders.inline_start);
}

TEST_F(NGLengthUtilsTest, testPadding) {
  style_->SetPaddingTop(Length(10, kPercent));
  style_->SetPaddingRight(Length(52, kFixed));
  style_->SetPaddingBottom(Length(kAuto));
  style_->SetPaddingLeft(Length(11, kPercent));
  style_->SetWritingMode(WritingMode::kVerticalRl);

  RefPtr<NGConstraintSpace> constraint_space(
      ConstructConstraintSpace(200, 300));

  NGBoxStrut padding = ComputePadding(*constraint_space, *style_);

  EXPECT_EQ(LayoutUnit(52), padding.block_start);
  EXPECT_EQ(LayoutUnit(), padding.inline_end);
  EXPECT_EQ(LayoutUnit(22), padding.block_end);
  EXPECT_EQ(LayoutUnit(20), padding.inline_start);
}

TEST_F(NGLengthUtilsTest, testAutoMargins) {
  style_->SetMarginRight(Length(kAuto));
  style_->SetMarginLeft(Length(kAuto));

  LayoutUnit kInlineSize = LayoutUnit(150);
  RefPtr<NGConstraintSpace> constraint_space(
      ConstructConstraintSpace(200, 300));

  NGBoxStrut margins;
  ApplyAutoMargins(*constraint_space, *style_, kInlineSize, &margins);

  EXPECT_EQ(LayoutUnit(), margins.block_start);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(25), margins.inline_start);
  EXPECT_EQ(LayoutUnit(25), margins.inline_end);

  style_->SetMarginLeft(Length(0, kFixed));
  margins = NGBoxStrut();
  ApplyAutoMargins(*constraint_space, *style_, kInlineSize, &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(50), margins.inline_end);

  style_->SetMarginLeft(Length(kAuto));
  style_->SetMarginRight(Length(0, kFixed));
  margins = NGBoxStrut();
  ApplyAutoMargins(*constraint_space, *style_, kInlineSize, &margins);
  EXPECT_EQ(LayoutUnit(50), margins.inline_start);
  EXPECT_EQ(LayoutUnit(0), margins.inline_end);

  // Test that we don't end up with negative "auto" margins when the box is too
  // big.
  style_->SetMarginLeft(Length(kAuto));
  style_->SetMarginRight(Length(5000, kFixed));
  margins = NGBoxStrut();
  margins.inline_end = LayoutUnit(5000);
  ApplyAutoMargins(*constraint_space, *style_, kInlineSize, &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(5000), margins.inline_end);
}

// Simple wrappers that don't use LayoutUnit(). Their only purpose is to make
// the tests below humanly readable (to make the expectation expressions fit on
// one line each). Passing 0 for column width or column count means "auto".
int GetUsedColumnWidth(int computed_column_count,
                       int computed_column_width,
                       int used_column_gap,
                       int available_inline_size) {
  LayoutUnit column_width(computed_column_width);
  if (!computed_column_width)
    column_width = LayoutUnit(NGSizeIndefinite);
  return ResolveUsedColumnInlineSize(computed_column_count, column_width,
                                     LayoutUnit(used_column_gap),
                                     LayoutUnit(available_inline_size))
      .ToInt();
}
int GetUsedColumnCount(int computed_column_count,
                       int computed_column_width,
                       int used_column_gap,
                       int available_inline_size) {
  LayoutUnit column_width(computed_column_width);
  if (!computed_column_width)
    column_width = LayoutUnit(NGSizeIndefinite);
  return ResolveUsedColumnCount(computed_column_count, column_width,
                                LayoutUnit(used_column_gap),
                                LayoutUnit(available_inline_size));
}

TEST_F(NGLengthUtilsTest, testColumnWidthAndCount) {
  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 0, 300));
  EXPECT_EQ(3, GetUsedColumnCount(0, 100, 0, 300));
  EXPECT_EQ(150, GetUsedColumnWidth(0, 101, 0, 300));
  EXPECT_EQ(2, GetUsedColumnCount(0, 101, 0, 300));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 151, 0, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 151, 0, 300));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 1000, 0, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 1000, 0, 300));

  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 10, 320));
  EXPECT_EQ(3, GetUsedColumnCount(0, 100, 10, 320));
  EXPECT_EQ(150, GetUsedColumnWidth(0, 101, 10, 310));
  EXPECT_EQ(2, GetUsedColumnCount(0, 101, 10, 310));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 151, 10, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 151, 10, 300));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 1000, 10, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 1000, 10, 300));

  EXPECT_EQ(125, GetUsedColumnWidth(4, 0, 0, 500));
  EXPECT_EQ(4, GetUsedColumnCount(4, 0, 0, 500));
  EXPECT_EQ(125, GetUsedColumnWidth(4, 100, 0, 500));
  EXPECT_EQ(4, GetUsedColumnCount(4, 100, 0, 500));
  EXPECT_EQ(100, GetUsedColumnWidth(6, 100, 0, 500));
  EXPECT_EQ(5, GetUsedColumnCount(6, 100, 0, 500));
  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 0, 500));
  EXPECT_EQ(5, GetUsedColumnCount(0, 100, 0, 500));

  EXPECT_EQ(125, GetUsedColumnWidth(4, 0, 10, 530));
  EXPECT_EQ(4, GetUsedColumnCount(4, 0, 10, 530));
  EXPECT_EQ(125, GetUsedColumnWidth(4, 100, 10, 530));
  EXPECT_EQ(4, GetUsedColumnCount(4, 100, 10, 530));
  EXPECT_EQ(100, GetUsedColumnWidth(6, 100, 10, 540));
  EXPECT_EQ(5, GetUsedColumnCount(6, 100, 10, 540));
  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 10, 540));
  EXPECT_EQ(5, GetUsedColumnCount(0, 100, 10, 540));
}

}  // namespace
}  // namespace blink
