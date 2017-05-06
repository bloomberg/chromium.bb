// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_space_utils.h"

#include "core/layout/ng/ng_base_layout_algorithm_test.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace {

class NGSpaceUtilsTest : public NGBaseLayoutAlgorithmTest {};

// Verifies that IsNewFormattingContextForInFlowBlockLevelChild returnes true
// if the child is out-of-flow, e.g. floating or abs-pos.
TEST_F(NGSpaceUtilsTest, NewFormattingContextForOutOfFlowChild) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="parent">
      <div id="child"></div>
    </div>
  )HTML");

  auto& parent_style = GetLayoutObjectByElementId("parent")->StyleRef();
  auto* child = GetLayoutObjectByElementId("child");
  auto* node = new NGBlockNode(child);

  auto run_test = [&](RefPtr<ComputedStyle> style) {
    child->SetStyle(style);
    EXPECT_TRUE(IsNewFormattingContextForBlockLevelChild(parent_style, *node));
  };

  RefPtr<ComputedStyle> style = ComputedStyle::Create();
  style->SetFloating(EFloat::kLeft);
  run_test(style);

  style = ComputedStyle::Create();
  style->SetPosition(EPosition::kAbsolute);
  run_test(style);

  style = ComputedStyle::Create();
  style->SetPosition(EPosition::kFixed);
  run_test(style);
}

}  // namespace
}  // namespace blink
