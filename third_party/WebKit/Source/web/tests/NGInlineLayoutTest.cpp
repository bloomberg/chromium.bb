// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/text/CharacterNames.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

class NGInlineLayoutTest : public SimTest {
 public:
  RefPtr<NGConstraintSpace> ConstraintSpaceForElement(
      LayoutNGBlockFlow* block_flow) {
    return NGConstraintSpaceBuilder(
               FromPlatformWritingMode(block_flow->Style()->GetWritingMode()))
        .SetAvailableSize(NGLogicalSize(LayoutUnit(), LayoutUnit()))
        .SetPercentageResolutionSize(NGLogicalSize(LayoutUnit(), LayoutUnit()))
        .SetTextDirection(block_flow->Style()->Direction())
        .ToConstraintSpace(
            FromPlatformWritingMode(block_flow->Style()->GetWritingMode()));
  }
};

TEST_F(NGInlineLayoutTest, BlockWithSingleTextNode) {
  RuntimeEnabledFeatures::setLayoutNGEnabled(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div id=\"target\">Hello <strong>World</strong>!</div>");

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetDocument().GetElementById("target");
  LayoutNGBlockFlow* block_flow =
      ToLayoutNGBlockFlow(target->GetLayoutObject());
  RefPtr<NGConstraintSpace> constraint_space =
      ConstraintSpaceForElement(block_flow);
  NGBlockNode* node = new NGBlockNode(block_flow);

  RefPtr<NGLayoutResult> result =
      NGBlockLayoutAlgorithm(node, constraint_space.Get()).Layout();
  EXPECT_TRUE(result);

  String expected_text("Hello World!");
  EXPECT_EQ(expected_text, ToNGInlineNode(node->FirstChild())->Text(0, 12));
}

TEST_F(NGInlineLayoutTest, BlockWithTextAndAtomicInline) {
  RuntimeEnabledFeatures::setLayoutNGEnabled(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id=\"target\">Hello <img>.</div>");

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetDocument().GetElementById("target");
  LayoutNGBlockFlow* block_flow =
      ToLayoutNGBlockFlow(target->GetLayoutObject());
  RefPtr<NGConstraintSpace> constraint_space =
      ConstraintSpaceForElement(block_flow);
  NGBlockNode* node = new NGBlockNode(block_flow);

  RefPtr<NGLayoutResult> result =
      NGBlockLayoutAlgorithm(node, constraint_space.Get()).Layout();
  EXPECT_TRUE(result);

  String expected_text("Hello ");
  expected_text.Append(kObjectReplacementCharacter);
  expected_text.Append(".");
  EXPECT_EQ(expected_text, ToNGInlineNode(node->FirstChild())->Text(0, 8));

  // Delete the line box tree to avoid leaks in the test.
  block_flow->DeleteLineBoxTree();
}

}  // namespace blink
