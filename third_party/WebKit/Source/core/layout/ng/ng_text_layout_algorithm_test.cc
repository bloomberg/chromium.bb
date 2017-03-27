// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_base_layout_algorithm_test.h"

#include "core/dom/TagCollection.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_physical_line_box_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {
namespace {

class NGTextLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {};

// Verifies that text can flow correctly around floats that were positioned
// before the inline block.
TEST_F(NGTextLayoutAlgorithmTest, TextFloatsAroundFloatsBefore) {
  setBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 20px;
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
  auto* line_box_fragments_wrapper =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  Vector<NGPhysicalTextFragment*> text_fragments;
  for (const auto& child : line_box_fragments_wrapper->Children()) {
    auto* line_box = toNGPhysicalLineBoxFragment(child.get());
    EXPECT_EQ(1u, line_box->Children().size());
    for (const auto& text : line_box->Children())
      text_fragments.push_back(toNGPhysicalTextFragment(text.get()));
  }

  LayoutText* layout_text =
      toLayoutText(getLayoutObjectByElementId("text")->slowFirstChild());
  ASSERT(layout_text->hasTextBoxes());

  ASSERT_EQ(4UL, text_fragments.size());

  auto* text_fragment1 = text_fragments[0];
  // 40 = #left-float1' width 30 + #left-float2 10
  EXPECT_EQ(LayoutUnit(40), text_fragment1->LeftOffset());
  EXPECT_EQ("The quick ", text_fragment1->Text());
  InlineTextBox* inline_text_box1 = layout_text->firstTextBox();
  EXPECT_EQ(LayoutUnit(40), inline_text_box1->x());

  auto* text_fragment2 = text_fragments[1];
  // 40 = #left-float1' width 30
  EXPECT_EQ(LayoutUnit(30), text_fragment2->LeftOffset());
  EXPECT_EQ("brown fox ", text_fragment2->Text());
  InlineTextBox* inline_text_box2 = inline_text_box1->nextTextBox();
  EXPECT_EQ(LayoutUnit(30), inline_text_box2->x());

  auto* text_fragment3 = text_fragments[2];
  EXPECT_EQ(LayoutUnit(), text_fragment3->LeftOffset());
  EXPECT_EQ("jumps over the lazy ", text_fragment3->Text());
  InlineTextBox* inline_text_box3 = inline_text_box2->nextTextBox();
  EXPECT_EQ(LayoutUnit(), inline_text_box3->x());
}

// Verifies that text correctly flows around the inline float that fits on
// the same text line.
TEST_F(NGTextLayoutAlgorithmTest, TextFloatsAroundInlineFloatThatFitsOnLine) {
  setBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #narrow-float {
        float: left; width: 30px; height: 30px; background-color: blue;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="narrow-float"></div> brown fox jumps over the lazy
      </span>
    </div>
  )HTML");
  LayoutText* layout_text =
      toLayoutText(getLayoutObjectByElementId("text")->slowFirstChild());
  ASSERT(layout_text->hasTextBoxes());

  InlineTextBox* inline_text_box1 = layout_text->firstTextBox();
  // 30 == narrow-float's width.
  EXPECT_EQ(LayoutUnit(30), inline_text_box1->x());

  Element* narrow_float = document().getElementById("narrow-float");
  // 8 == body's margin.
  EXPECT_EQ(8, narrow_float->offsetLeft());
  EXPECT_EQ(8, narrow_float->offsetTop());
}

// Verifies that the inline float got pushed to the next line if it doesn't
// fit the current line.
TEST_F(NGTextLayoutAlgorithmTest,
       TextFloatsAroundInlineFloatThatDoesNotFitOnLine) {
  setBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #wide-float {
        float: left; width: 160px; height: 30px; background-color: red;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="wide-float"></div> brown fox jumps over the lazy dog
      </span>
    </div>
  )HTML");
  LayoutText* layout_text =
      toLayoutText(getLayoutObjectByElementId("text")->slowFirstChild());
  ASSERT(layout_text->hasTextBoxes());

  InlineTextBox* inline_text_box1 = layout_text->firstTextBox();
  EXPECT_EQ(LayoutUnit(), inline_text_box1->x());

  Element* wide_float = document().getElementById("wide-float");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->offsetLeft());
}

// Verifies that if an inline float pushed to the next line then all others
// following inline floats positioned with respect to the float's top edge
// alignment rule.
TEST_F(NGTextLayoutAlgorithmTest,
       FloatsArePositionedWithRespectToTopEdgeAlignmentRule) {
  setBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #left-narrow {
        float: left; width: 5px; height: 30px; background-color: blue;
      }
      #left-wide {
        float: left; width: 160px; height: 30px; background-color: red;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="left-wide"></div> brown <div id="left-narrow"></div>
        fox jumps over the lazy dog
      </span>
    </div>
  )HTML");
  Element* wide_float = document().getElementById("left-wide");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->offsetLeft());

  Element* narrow_float = document().getElementById("left-narrow");
  // 160 float-wide's width + 8 body's margin.
  EXPECT_EQ(160 + 8, narrow_float->offsetLeft());

  // On the same line.
  EXPECT_EQ(wide_float->offsetTop(), narrow_float->offsetTop());
}

}  // namespace
}  // namespace blink
