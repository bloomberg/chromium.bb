// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/WritingModeUtils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

enum { kTop, kRight, kBottom, kLeft };

void CheckLegacyLogicalDirections(PhysicalToLogical<int> converter) {
  EXPECT_EQ(converter.InlineStart(), converter.Start());
  EXPECT_EQ(converter.InlineEnd(), converter.End());
  EXPECT_EQ(converter.BlockStart(), converter.Before());
  EXPECT_EQ(converter.BlockEnd(), converter.After());
}

TEST(WritingModeUtilsTest, PhysicalToLogicalHorizontalLtr) {
  PhysicalToLogical<int> converter(WritingMode::kHorizontalTb,
                                   TextDirection::kLtr, kTop, kRight, kBottom,
                                   kLeft);
  EXPECT_EQ(kLeft, converter.InlineStart());
  EXPECT_EQ(kRight, converter.InlineEnd());
  EXPECT_EQ(kTop, converter.BlockStart());
  EXPECT_EQ(kBottom, converter.BlockEnd());
  EXPECT_EQ(kLeft, converter.LineLeft());
  EXPECT_EQ(kRight, converter.LineRight());
  EXPECT_EQ(kTop, converter.Over());
  EXPECT_EQ(kBottom, converter.Under());
  CheckLegacyLogicalDirections(converter);
}

TEST(WritingModeUtilsTest, PhysicalToLogicalHorizontalRtl) {
  PhysicalToLogical<int> converter(WritingMode::kHorizontalTb,
                                   TextDirection::kRtl, kTop, kRight, kBottom,
                                   kLeft);
  EXPECT_EQ(kRight, converter.InlineStart());
  EXPECT_EQ(kLeft, converter.InlineEnd());
  EXPECT_EQ(kTop, converter.BlockStart());
  EXPECT_EQ(kBottom, converter.BlockEnd());
  EXPECT_EQ(kLeft, converter.LineLeft());
  EXPECT_EQ(kRight, converter.LineRight());
  EXPECT_EQ(kTop, converter.Over());
  EXPECT_EQ(kBottom, converter.Under());
  CheckLegacyLogicalDirections(converter);
}

TEST(WritingModeUtilsTest, PhysicalToLogicalVlrLtr) {
  PhysicalToLogical<int> converter(WritingMode::kVerticalLr,
                                   TextDirection::kLtr, kTop, kRight, kBottom,
                                   kLeft);
  EXPECT_EQ(kTop, converter.InlineStart());
  EXPECT_EQ(kBottom, converter.InlineEnd());
  EXPECT_EQ(kLeft, converter.BlockStart());
  EXPECT_EQ(kRight, converter.BlockEnd());
  EXPECT_EQ(kTop, converter.LineLeft());
  EXPECT_EQ(kBottom, converter.LineRight());
  EXPECT_EQ(kRight, converter.Over());
  EXPECT_EQ(kLeft, converter.Under());
  CheckLegacyLogicalDirections(converter);
}

TEST(WritingModeUtilsTest, PhysicalToLogicalVlrRtl) {
  PhysicalToLogical<int> converter(WritingMode::kVerticalLr,
                                   TextDirection::kRtl, kTop, kRight, kBottom,
                                   kLeft);
  EXPECT_EQ(kBottom, converter.InlineStart());
  EXPECT_EQ(kTop, converter.InlineEnd());
  EXPECT_EQ(kLeft, converter.BlockStart());
  EXPECT_EQ(kRight, converter.BlockEnd());
  EXPECT_EQ(kTop, converter.LineLeft());
  EXPECT_EQ(kBottom, converter.LineRight());
  EXPECT_EQ(kRight, converter.Over());
  EXPECT_EQ(kLeft, converter.Under());
  CheckLegacyLogicalDirections(converter);
}

TEST(WritingModeUtilsTest, PhysicalToLogicalVrlLtr) {
  PhysicalToLogical<int> converter(WritingMode::kVerticalRl,
                                   TextDirection::kLtr, kTop, kRight, kBottom,
                                   kLeft);
  EXPECT_EQ(kTop, converter.InlineStart());
  EXPECT_EQ(kBottom, converter.InlineEnd());
  EXPECT_EQ(kRight, converter.BlockStart());
  EXPECT_EQ(kLeft, converter.BlockEnd());
  EXPECT_EQ(kTop, converter.LineLeft());
  EXPECT_EQ(kBottom, converter.LineRight());
  EXPECT_EQ(kRight, converter.Over());
  EXPECT_EQ(kLeft, converter.Under());
  CheckLegacyLogicalDirections(converter);
}

TEST(WritingModeUtilsTest, PhysicalToLogicalVrlRtl) {
  PhysicalToLogical<int> converter(WritingMode::kVerticalRl,
                                   TextDirection::kRtl, kTop, kRight, kBottom,
                                   kLeft);
  EXPECT_EQ(kBottom, converter.InlineStart());
  EXPECT_EQ(kTop, converter.InlineEnd());
  EXPECT_EQ(kRight, converter.BlockStart());
  EXPECT_EQ(kLeft, converter.BlockEnd());
  EXPECT_EQ(kTop, converter.LineLeft());
  EXPECT_EQ(kBottom, converter.LineRight());
  EXPECT_EQ(kRight, converter.Over());
  EXPECT_EQ(kLeft, converter.Under());
  CheckLegacyLogicalDirections(converter);
}

enum { kInlineStart, kInlineEnd, kBlockStart, kBlockEnd };

TEST(WritingModeUtilsTest, LogicalToPhysicalHorizontalLtr) {
  LogicalToPhysical<int> converter(WritingMode::kHorizontalTb,
                                   TextDirection::kLtr, kInlineStart,
                                   kInlineEnd, kBlockStart, kBlockEnd);
  EXPECT_EQ(kInlineStart, converter.Left());
  EXPECT_EQ(kInlineEnd, converter.Right());
  EXPECT_EQ(kBlockStart, converter.Top());
  EXPECT_EQ(kBlockEnd, converter.Bottom());
}

TEST(WritingModeUtilsTest, LogicalToPhysicalHorizontalRtl) {
  LogicalToPhysical<int> converter(WritingMode::kHorizontalTb,
                                   TextDirection::kRtl, kInlineStart,
                                   kInlineEnd, kBlockStart, kBlockEnd);
  EXPECT_EQ(kInlineEnd, converter.Left());
  EXPECT_EQ(kInlineStart, converter.Right());
  EXPECT_EQ(kBlockStart, converter.Top());
  EXPECT_EQ(kBlockEnd, converter.Bottom());
}

TEST(WritingModeUtilsTest, LogicalToPhysicalVlrLtr) {
  LogicalToPhysical<int> converter(WritingMode::kVerticalLr,
                                   TextDirection::kLtr, kInlineStart,
                                   kInlineEnd, kBlockStart, kBlockEnd);
  EXPECT_EQ(kBlockStart, converter.Left());
  EXPECT_EQ(kBlockEnd, converter.Right());
  EXPECT_EQ(kInlineStart, converter.Top());
  EXPECT_EQ(kInlineEnd, converter.Bottom());
}

TEST(WritingModeUtilsTest, LogicalToPhysicalVlrRtl) {
  LogicalToPhysical<int> converter(WritingMode::kVerticalLr,
                                   TextDirection::kRtl, kInlineStart,
                                   kInlineEnd, kBlockStart, kBlockEnd);
  EXPECT_EQ(kBlockStart, converter.Left());
  EXPECT_EQ(kBlockEnd, converter.Right());
  EXPECT_EQ(kInlineEnd, converter.Top());
  EXPECT_EQ(kInlineStart, converter.Bottom());
}

TEST(WritingModeUtilsTest, LogicalToPhysicalVrlLtr) {
  LogicalToPhysical<int> converter(WritingMode::kVerticalRl,
                                   TextDirection::kLtr, kInlineStart,
                                   kInlineEnd, kBlockStart, kBlockEnd);
  EXPECT_EQ(kBlockEnd, converter.Left());
  EXPECT_EQ(kBlockStart, converter.Right());
  EXPECT_EQ(kInlineStart, converter.Top());
  EXPECT_EQ(kInlineEnd, converter.Bottom());
}

TEST(WritingModeUtilsTest, LogicalToPhysicalVrlRtl) {
  LogicalToPhysical<int> converter(WritingMode::kVerticalRl,
                                   TextDirection::kRtl, kInlineStart,
                                   kInlineEnd, kBlockStart, kBlockEnd);
  EXPECT_EQ(kBlockEnd, converter.Left());
  EXPECT_EQ(kBlockStart, converter.Right());
  EXPECT_EQ(kInlineEnd, converter.Top());
  EXPECT_EQ(kInlineStart, converter.Bottom());
}

}  // namespace

}  // namespace blink
