// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/ng/ng_fragment.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {
class NGBlockNodeForTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockNodeForTest, MinAndMaxContent) {
  const int kWidth = 30;

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(kWidth, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  NGBlockNode* box = new NGBlockNode(style_.get());
  box->SetFirstChild(first_child);

  MinAndMaxContentSizes sizes;
  while (!box->ComputeMinAndMaxContentSizes(&sizes))
    continue;
  EXPECT_EQ(LayoutUnit(kWidth), sizes.min_content);
  EXPECT_EQ(LayoutUnit(kWidth), sizes.max_content);
}
}  // namespace
}  // namespace blink
