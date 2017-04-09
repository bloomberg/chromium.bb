// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_min_max_content_size.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {
class NGBlockNodeForTest : public RenderingTest {
 public:
  NGBlockNodeForTest() { RuntimeEnabledFeatures::setLayoutNGEnabled(true); }
  ~NGBlockNodeForTest() { RuntimeEnabledFeatures::setLayoutNGEnabled(false); };
};

TEST_F(NGBlockNodeForTest, MinAndMaxContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="box" >
      <div id="first_child" style="width:30px">
      </div>
    </div>
  )HTML");
  const int kWidth = 30;

  NGBlockNode* box = new NGBlockNode(GetLayoutObjectByElementId("box"));
  MinMaxContentSize sizes = box->ComputeMinMaxContentSize();
  EXPECT_EQ(LayoutUnit(kWidth), sizes.min_content);
  EXPECT_EQ(LayoutUnit(kWidth), sizes.max_content);
}
}  // namespace
}  // namespace blink
