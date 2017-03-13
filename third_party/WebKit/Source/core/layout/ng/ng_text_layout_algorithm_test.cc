// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_base_layout_algorithm_test.h"

#include "core/dom/TagCollection.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {
namespace {

class NGTextLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {};

// Verifies that text can flow correctly around floats that were positioned
// before the inline block.
// Failing on Android: crbug.com/700868
#if OS(ANDROID)
#define MAYBE_TextFloatsAroundFloatsBefore DISABLED_TextFloatsAroundFloatsBefore
#else
#define MAYBE_TextFloatsAroundFloatsBefore TextFloatsAroundFloatsBefore
#endif
TEST_F(NGTextLayoutAlgorithmTest, MAYBE_TextFloatsAroundFloatsBefore) {
  setBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid blue;
      }
      #left-float1 {
        float: left; width: 30px; height: 30px; background-color: blue;
      }
      #left-float2 {
        float: left; width: 10px; height: 10px;
        background-color: purple;
      }
      #right-float {
        float: right; width: 40px; height: 40px; background-color: yellow;
      }
    </style>
    <div id="container">
      <div id="left-float1"></div>
      <div id="left-float2"></div>
      <div id="right-float"></div>
      <span id="text">The quick brown fox jumps over the lazy dog</span>
    </div>
  )HTML");
  // ** Run LayoutNG algorithm **
  RefPtr<NGConstraintSpace> space;
  RefPtr<NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      toNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  auto* text_fragments_wrapper =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());

  LayoutText* layout_text =
      toLayoutText(getLayoutObjectByElementId("text")->slowFirstChild());
  ASSERT(layout_text->hasTextBoxes());

  // TODO(glebl): Should have only 3 text fragments. For some reason we have a
  // left over fragment with text == "dog".
  ASSERT_EQ(4UL, text_fragments_wrapper->Children().size());

  auto* text_fragment1 =
      toNGPhysicalTextFragment(text_fragments_wrapper->Children()[0].get());
  auto* text_node = text_fragment1->Node();
  // 40 = #left-float1' width 30 + #left-float2 10
  EXPECT_EQ(LayoutUnit(40), text_fragment1->LeftOffset());
  EXPECT_EQ("The quick ", text_node->Text(text_fragment1->StartOffset(),
                                          text_fragment1->EndOffset()));
  InlineTextBox* inline_text_box1 = layout_text->firstTextBox();
  EXPECT_EQ(LayoutPoint(40, 0), inline_text_box1->frameRect().location());

  auto* text_fragment2 =
      toNGPhysicalTextFragment(text_fragments_wrapper->Children()[1].get());
  // 40 = #left-float1' width 30
  EXPECT_EQ(LayoutUnit(30), text_fragment2->LeftOffset());
  EXPECT_EQ("brown fox jumps over",
            text_node->Text(text_fragment2->StartOffset(),
                            text_fragment2->EndOffset()));
  InlineTextBox* inline_text_box2 = inline_text_box1->nextTextBox();
  EXPECT_EQ(LayoutPoint(30, 22), inline_text_box2->frameRect().location());

  auto* text_fragment3 =
      toNGPhysicalTextFragment(text_fragments_wrapper->Children()[2].get());
  EXPECT_EQ(LayoutUnit(), text_fragment3->LeftOffset());
  EXPECT_EQ("jumps over the lazy dog",
            text_node->Text(text_fragment3->StartOffset(),
                            text_fragment3->EndOffset()));
  InlineTextBox* inline_text_box3 = inline_text_box2->nextTextBox();
  EXPECT_EQ(LayoutPoint(0, 44), inline_text_box3->frameRect().location());
}

}  // namespace
}  // namespace blink
