// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_absolute_utils.h"

#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#define NGAuto LayoutUnit(-1)

class NGAbsoluteUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    style_ = ComputedStyle::create();
    // If not set, border widths will always be 0.
    style_->setBorderLeftStyle(EBorderStyle::BorderStyleSolid);
    style_->setBorderRightStyle(EBorderStyle::BorderStyleSolid);
    style_->setBorderTopStyle(EBorderStyle::BorderStyleSolid);
    style_->setBorderBottomStyle(EBorderStyle::BorderStyleSolid);
    style_->setBoxSizing(EBoxSizing::BoxSizingBorderBox);
    container_size_ = NGLogicalSize(LayoutUnit(200), LayoutUnit(300));
    NGConstraintSpaceBuilder builder(kHorizontalTopBottom);
    builder.SetAvailableSize(container_size_);
    ltr_space_ = builder.SetWritingMode(kHorizontalTopBottom)
                     .SetTextDirection(LTR)
                     .ToConstraintSpace();
    rtl_space_ = builder.SetWritingMode(kHorizontalTopBottom)
                     .SetTextDirection(RTL)
                     .ToConstraintSpace();
    vertical_lr_space_ = builder.SetWritingMode(kVerticalLeftRight)
                             .SetTextDirection(LTR)
                             .ToConstraintSpace();
    vertical_rl_space_ = builder.SetWritingMode(kVerticalLeftRight)
                             .SetTextDirection(LTR)
                             .ToConstraintSpace();
  }

  void SetHorizontalStyle(LayoutUnit left,
                          LayoutUnit margin_left,
                          LayoutUnit width,
                          LayoutUnit margin_right,
                          LayoutUnit right) {
    style_->setLeft(left == NGAuto ? Length(LengthType::Auto)
                                   : Length(left.toInt(), LengthType::Fixed));
    style_->setMarginLeft(margin_left == NGAuto
                              ? Length(LengthType::Auto)
                              : Length(margin_left.toInt(), LengthType::Fixed));
    style_->setWidth(width == NGAuto
                         ? Length(LengthType::Auto)
                         : Length(width.toInt(), LengthType::Fixed));
    style_->setMarginRight(margin_right == NGAuto ? Length(LengthType::Auto)
                                                  : Length(margin_right.toInt(),
                                                           LengthType::Fixed));
    style_->setRight(right == NGAuto
                         ? Length(LengthType::Auto)
                         : Length(right.toInt(), LengthType::Fixed));
  }

  void SetVerticalStyle(LayoutUnit top,
                        LayoutUnit margin_top,
                        LayoutUnit height,
                        LayoutUnit margin_bottom,
                        LayoutUnit bottom) {
    style_->setTop(top == NGAuto ? Length(LengthType::Auto)
                                 : Length(top.toInt(), LengthType::Fixed));
    style_->setMarginTop(margin_top == NGAuto
                             ? Length(LengthType::Auto)
                             : Length(margin_top.toInt(), LengthType::Fixed));
    style_->setHeight(height == NGAuto
                          ? Length(LengthType::Auto)
                          : Length(height.toInt(), LengthType::Fixed));
    style_->setMarginBottom(
        margin_bottom == NGAuto
            ? Length(LengthType::Auto)
            : Length(margin_bottom.toInt(), LengthType::Fixed));
    style_->setBottom(bottom == NGAuto
                          ? Length(LengthType::Auto)
                          : Length(bottom.toInt(), LengthType::Fixed));
  }

  RefPtr<ComputedStyle> style_;
  NGLogicalSize container_size_;
  Persistent<NGConstraintSpace> ltr_space_;
  Persistent<NGConstraintSpace> rtl_space_;
  Persistent<NGConstraintSpace> vertical_lr_space_;
  Persistent<NGConstraintSpace> vertical_rl_space_;
};

TEST_F(NGAbsoluteUtilsTest, Horizontal) {
  // Test that the equation is computed correctly:
  // left + marginLeft + borderLeft  + paddingLeft +
  // width +
  // right + marginRight + borderRight + paddingRight = container width

  // Common setup.
  LayoutUnit left(5);
  LayoutUnit margin_left(7);
  LayoutUnit border_left(9);
  LayoutUnit padding_left(11);
  LayoutUnit right(13);
  LayoutUnit margin_right(15);
  LayoutUnit border_right(17);
  LayoutUnit padding_right(19);

  LayoutUnit width =
      container_size_.inline_size - left - margin_left - right - margin_right;

  Optional<LayoutUnit> estimated_inline;

  style_->setBorderLeftWidth(border_left.toInt());
  style_->setBorderRightWidth(border_right.toInt());
  style_->setPaddingLeft(Length(padding_left.toInt(), LengthType::Fixed));
  style_->setPaddingRight(Length(padding_right.toInt(), LengthType::Fixed));

  // These default to 3 which is not what we want.
  style_->setBorderBottomWidth(0);
  style_->setBorderTopWidth(0);

  NGAbsolutePhysicalPosition p;

  //
  // Tests.
  //

  // All auto => width is estimated_inline, left is 0.
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = LayoutUnit(60);
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(*estimated_inline, p.size.width);
  EXPECT_EQ(LayoutUnit(0), p.inset.left);

  // All auto + RTL.
  p = ComputePartialAbsoluteWithChildInlineSize(*rtl_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(*estimated_inline, p.size.width);
  EXPECT_EQ(LayoutUnit(0), p.inset.right);

  // left, right, and left are known, compute margins.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  LayoutUnit marginSpace =
      (container_size_.inline_size - left - right - p.size.width) / 2;
  EXPECT_EQ(left + marginSpace, p.inset.left);
  EXPECT_EQ(right + marginSpace, p.inset.right);

  // left, right, and left are known, compute margins, writing mode vertical_lr.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*vertical_lr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(left + marginSpace, p.inset.left);
  EXPECT_EQ(right + marginSpace, p.inset.right);

  // left, right, and left are known, compute margins, writing mode vertical_rl.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*vertical_rl_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(left + marginSpace, p.inset.left);
  EXPECT_EQ(right + marginSpace, p.inset.right);

  // left, right, and width are known, not enough space for margins LTR.
  SetHorizontalStyle(left, NGAuto, LayoutUnit(200), NGAuto, right);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(left, p.inset.left);
  EXPECT_EQ(-left, p.inset.right);

  // left, right, and left are known, not enough space for margins RTL.
  SetHorizontalStyle(left, NGAuto, LayoutUnit(200), NGAuto, right);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*rtl_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(-right, p.inset.left);
  EXPECT_EQ(right, p.inset.right);

  // Rule 1 left and width are auto.
  SetHorizontalStyle(NGAuto, margin_left, NGAuto, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = LayoutUnit(60);
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(*estimated_inline, p.size.width);

  // Rule 2 left and right are auto LTR.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(margin_left, p.inset.left);
  EXPECT_EQ(container_size_.inline_size - margin_left - width, p.inset.right);

  // Rule 2 left and right are auto RTL.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*rtl_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(container_size_.inline_size - margin_right - width, p.inset.left);
  EXPECT_EQ(margin_right, p.inset.right);

  // Rule 3 width and right are auto.
  SetHorizontalStyle(left, margin_left, NGAuto, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = LayoutUnit(60);
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(
      container_size_.inline_size - *estimated_inline - left - margin_left,
      p.inset.right);
  EXPECT_EQ(*estimated_inline, p.size.width);

  // Rule 4: left is auto.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(left + margin_left, p.inset.left);

  // Rule 4: left is auto, EBoxSizing::BoxSizingContentBox
  style_->setBoxSizing(EBoxSizing::BoxSizingContentBox);
  SetHorizontalStyle(NGAuto, margin_left, width - border_left - border_right -
                                              padding_left - padding_right,
                     margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(left + margin_left, p.inset.left);
  style_->setBoxSizing(EBoxSizing::BoxSizingBorderBox);

  // Rule 5: right is auto.
  SetHorizontalStyle(left, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(right + margin_right, p.inset.right);

  // Rule 6: width is auto.
  SetHorizontalStyle(left, margin_left, NGAuto, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(*ltr_space_, *style_,
                                                estimated_inline);
  EXPECT_EQ(width, p.size.width);
}

TEST_F(NGAbsoluteUtilsTest, Vertical) {
  // Test that the equation is computed correctly:
  // top + marginTop + borderTop  + paddingTop +
  // height +
  // bottom + marginBottom + borderBottom + paddingBottom = container height

  // Common setup
  LayoutUnit top(5);
  LayoutUnit margin_top(7);
  LayoutUnit border_top(9);
  LayoutUnit padding_top(11);
  LayoutUnit bottom(13);
  LayoutUnit margin_bottom(15);
  LayoutUnit border_bottom(17);
  LayoutUnit padding_bottom(19);

  LayoutUnit height =
      container_size_.block_size - top - margin_top - bottom - margin_bottom;

  style_->setBorderTopWidth(border_top.toInt());
  style_->setBorderBottomWidth(border_bottom.toInt());
  style_->setPaddingTop(Length(padding_top.toInt(), LengthType::Fixed));
  style_->setPaddingBottom(Length(padding_bottom.toInt(), LengthType::Fixed));
  // These default to 3 which is not what we want.
  style_->setBorderLeftWidth(0);
  style_->setBorderRightWidth(0);

  NGAbsolutePhysicalPosition p;
  Optional<LayoutUnit> auto_height;

  //
  // Tests
  //

  // All auto, compute margins.
  SetVerticalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(60);
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(*auto_height, p.size.height);
  EXPECT_EQ(LayoutUnit(0), p.inset.top);

  // If top, bottom, and height are known, compute margins.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  LayoutUnit margin_space =
      (container_size_.block_size - top - height - bottom) / 2;
  EXPECT_EQ(top + margin_space, p.inset.top);
  EXPECT_EQ(bottom + margin_space, p.inset.bottom);

  // If top, bottom, and height are known, writing mode vertical_lr.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  ComputeFullAbsoluteWithChildBlockSize(*vertical_lr_space_, *style_,
                                        auto_height, &p);
  EXPECT_EQ(top + margin_space, p.inset.top);
  EXPECT_EQ(bottom + margin_space, p.inset.bottom);

  // If top, bottom, and height are known, writing mode vertical_rl.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  ComputeFullAbsoluteWithChildBlockSize(*vertical_rl_space_, *style_,
                                        auto_height, &p);
  EXPECT_EQ(top + margin_space, p.inset.top);
  EXPECT_EQ(bottom + margin_space, p.inset.bottom);

  // If top, bottom, and height are known, not enough space for margins.
  SetVerticalStyle(top, NGAuto, LayoutUnit(300), NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(top, p.inset.top);
  EXPECT_EQ(-top, p.inset.bottom);

  // Rule 1: top and height are unknown.
  SetVerticalStyle(NGAuto, margin_top, NGAuto, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(60);
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(*auto_height, p.size.height);

  // Rule 2: top and bottom are unknown.
  SetVerticalStyle(NGAuto, margin_top, height, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(margin_top, p.inset.top);
  EXPECT_EQ(container_size_.block_size - margin_top - height, p.inset.bottom);

  // Rule 3: height and bottom are unknown, auto_height < border_padding.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(20);
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(border_top + border_bottom + padding_top + padding_bottom,
            p.size.height);

  // Rule 3: height and bottom are unknown.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(70);
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(*auto_height, p.size.height);

  // Rule 4: top is unknown.
  SetVerticalStyle(NGAuto, margin_top, height, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(top + margin_top, p.inset.top);

  // Rule 5: bottom is unknown.
  SetVerticalStyle(top, margin_top, height, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(bottom + margin_bottom, p.inset.bottom);

  // Rule 6: height is unknown.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(*ltr_space_, *style_, auto_height, &p);
  EXPECT_EQ(height, p.size.height);
}

}  // namespace
}  // namespace blink
