// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_space_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class NGSpaceUtilsTest : public ::testing::Test {
 protected:
  NGSpaceUtilsTest() : space_builder_(NGWritingMode::kHorizontalTopBottom) {}

  void SetUp() override { style_ = ComputedStyle::create(); }
  RefPtr<ComputedStyle> style_;
  NGConstraintSpaceBuilder space_builder_;
};

// Verifies that IsNewFormattingContextForInFlowBlockLevelChild returnes true
// if the child is out-of-flow, e.g. floating or abs-pos.
TEST_F(NGSpaceUtilsTest, NewFormattingContextForOutOfFlowChild) {
  auto run_test = [&](const ComputedStyle& style) {
    RefPtr<NGConstraintSpace> not_used_space =
        space_builder_.ToConstraintSpace(NGWritingMode::kHorizontalTopBottom);
    EXPECT_TRUE(
        IsNewFormattingContextForBlockLevelChild(*not_used_space.get(), style));
  };
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  style->setFloating(EFloat::kLeft);
  run_test(*style.get());

  style = ComputedStyle::create();
  style->setPosition(EPosition::kAbsolute);
  run_test(*style.get());

  style = ComputedStyle::create();
  style->setPosition(EPosition::kFixed);
  run_test(*style.get());
}

}  // namespace
}  // namespace blink
