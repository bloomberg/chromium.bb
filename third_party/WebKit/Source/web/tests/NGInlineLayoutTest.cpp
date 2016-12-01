// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_inline_layout_algorithm.h"
#include "core/layout/ng/ng_inline_node.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

class NGInlineLayoutTest : public SimTest {
 public:
  NGConstraintSpace* constraintSpaceForElement(LayoutNGBlockFlow* blockFlow) {
    return NGConstraintSpaceBuilder(
               FromPlatformWritingMode(blockFlow->style()->getWritingMode()))
        .SetAvailableSize(NGLogicalSize(LayoutUnit(), LayoutUnit()))
        .SetPercentageResolutionSize(NGLogicalSize(LayoutUnit(), LayoutUnit()))
        .SetTextDirection(blockFlow->style()->direction())
        .ToConstraintSpace();
  }
};

TEST_F(NGInlineLayoutTest, BlockWithSingleTextNode) {
  RuntimeEnabledFeatures::setLayoutNGEnabled(true);
  RuntimeEnabledFeatures::setLayoutNGInlineEnabled(true);

  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");
  mainResource.complete(
      "<div id=\"target\">Hello <strong>World</strong>!</div>");

  compositor().beginFrame();
  ASSERT_FALSE(compositor().needsBeginFrame());

  Element* target = document().getElementById("target");
  LayoutNGBlockFlow* blockFlow = toLayoutNGBlockFlow(target->layoutObject());
  NGConstraintSpace* constraintSpace = constraintSpaceForElement(blockFlow);

  NGInlineNode* inlineBox =
      new NGInlineNode(blockFlow->firstChild(), blockFlow->mutableStyle());
  NGInlineLayoutAlgorithm* layoutAlgorithm = new NGInlineLayoutAlgorithm(
      blockFlow->style(), inlineBox, constraintSpace);

  String expectedText("Hello World!");
  EXPECT_EQ(expectedText, inlineBox->Text(0, 12));

  NGPhysicalFragmentBase* fragment;
  layoutAlgorithm->Layout(nullptr, &fragment, nullptr);
}

TEST_F(NGInlineLayoutTest, BlockWithTextAndAtomicInline) {
  RuntimeEnabledFeatures::setLayoutNGEnabled(true);
  RuntimeEnabledFeatures::setLayoutNGInlineEnabled(true);

  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");
  mainResource.complete("<div id=\"target\">Hello <img>.</div>");

  compositor().beginFrame();
  ASSERT_FALSE(compositor().needsBeginFrame());

  Element* target = document().getElementById("target");
  LayoutNGBlockFlow* blockFlow = toLayoutNGBlockFlow(target->layoutObject());
  NGConstraintSpace* constraintSpace = constraintSpaceForElement(blockFlow);

  NGInlineNode* inlineBox =
      new NGInlineNode(blockFlow->firstChild(), blockFlow->mutableStyle());
  NGInlineLayoutAlgorithm* layoutAlgorithm = new NGInlineLayoutAlgorithm(
      blockFlow->style(), inlineBox, constraintSpace);

  String expectedText("Hello ");
  expectedText.append(objectReplacementCharacter);
  expectedText.append(".");
  EXPECT_EQ(expectedText, inlineBox->Text(0, 8));

  NGPhysicalFragmentBase* fragment;
  layoutAlgorithm->Layout(nullptr, &fragment, nullptr);
}

}  // namespace blink
