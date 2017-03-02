// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {
class NGBlockNodeForTest : public RenderingTest {
 public:
  NGBlockNodeForTest() { RuntimeEnabledFeatures::setLayoutNGEnabled(true); }
  ~NGBlockNodeForTest() { RuntimeEnabledFeatures::setLayoutNGEnabled(false); };
};

TEST_F(NGBlockNodeForTest, MinAndMaxContent) {
  setBodyInnerHTML(R"HTML(
    <div id="box" >
      <div id="first_child" style="width:30px">
      </div>
    </div>
  )HTML");
  const int kWidth = 30;

  NGBlockNode* box = new NGBlockNode(getLayoutObjectByElementId("box"));
  MinAndMaxContentSizes sizes = box->ComputeMinAndMaxContentSizes();
  EXPECT_EQ(LayoutUnit(kWidth), sizes.min_content);
  EXPECT_EQ(LayoutUnit(kWidth), sizes.max_content);
}
}  // namespace
}  // namespace blink
