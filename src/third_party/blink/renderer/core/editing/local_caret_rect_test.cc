// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

bool operator==(const LocalCaretRect& rect1, const LocalCaretRect& rect2) {
  return rect1.layout_object == rect2.layout_object && rect1.rect == rect2.rect;
}

std::ostream& operator<<(std::ostream& out, const LocalCaretRect& caret_rect) {
  return out << "layout_object = " << caret_rect.layout_object
             << ", rect = " << caret_rect.rect;
}

class LocalCaretRectTest : public EditingTestBase {};

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLocalCaretRectTest
    : public testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      public LocalCaretRectTest {
 public:
  ParameterizedLocalCaretRectTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }

  LocalCaretRect LocalCaretRectOf(
      const Position& position,
      EditingBoundaryCrossingRule rule = kCanCrossEditingBoundary) {
    return LocalCaretRectOfPosition(PositionWithAffinity(position), rule);
  }
};

INSTANTIATE_TEST_SUITE_P(All, ParameterizedLocalCaretRectTest, testing::Bool());

TEST_P(ParameterizedLocalCaretRectTest, DOMAndFlatTrees) {
  const char* body_content =
      "<p id='host'><b slot='#one' id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><slot name=#one></slot><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");

  const LocalCaretRect& caret_rect_from_dom_tree = LocalCaretRectOfPosition(
      PositionWithAffinity(Position(one->firstChild(), 0)));

  const LocalCaretRect& caret_rect_from_flat_tree = LocalCaretRectOfPosition(
      PositionInFlatTreeWithAffinity(PositionInFlatTree(one->firstChild(), 0)));

  EXPECT_FALSE(caret_rect_from_dom_tree.IsEmpty());
  EXPECT_EQ(caret_rect_from_dom_tree, caret_rect_from_flat_tree);
}

// http://crbug.com/1174101
TEST_P(ParameterizedLocalCaretRectTest, EmptyInlineFlex) {
  LoadAhem();
  InsertStyleElement(R"CSS(
    div { font: 10px/15px Ahem; width: 100px; }
    i {
        display: inline-flex;
        width: 30px; height: 30px;
        border: solid 10px red;
    })CSS");
  // |ComputeInlinePosition(AfterChildren:<div>)=AfterChildren:<b>
  // When removing <i>, we have <b>@0
  SetBodyContent(
      "<div id=target contenteditable>"
      "ab<i contenteditable=false><b></b></i></div>");
  const auto& target = *GetElementById("target");
  const auto& ab = *To<Text>(target.firstChild());
  const auto& inline_flex = *ab.nextSibling();
  const LocalCaretRect before_ab =
      LocalCaretRect(ab.GetLayoutObject(), {0, 32, 1, 10});
  const LocalCaretRect before_inline_flex =
      // LayoutNG is correct. legacy layout places caret inside inline-flex.
      LayoutNGEnabled()
          ? LocalCaretRect(ab.GetLayoutObject(), {20, 32, 1, 10})
          : LocalCaretRect(inline_flex.GetLayoutObject(), {10, 10, 1, 50});
  const LocalCaretRect after_inline_flex =
      // LayoutNG is correct. legacy layout places caret inside inline-flex.
      LayoutNGEnabled()
          ? LocalCaretRect(inline_flex.GetLayoutObject(), {49, 0, 1, 50})
          : LocalCaretRect(inline_flex.GetLayoutObject(), {59, 10, 1, 50});

  EXPECT_EQ(before_ab, LocalCaretRectOf(Position(target, 0)));
  EXPECT_EQ(before_inline_flex, LocalCaretRectOf(Position(target, 1)));
  EXPECT_EQ(after_inline_flex, LocalCaretRectOf(Position(target, 2)));
  EXPECT_EQ(before_ab, LocalCaretRectOf(Position::BeforeNode(target)));
  EXPECT_EQ(after_inline_flex, LocalCaretRectOf(Position::AfterNode(target)));
  EXPECT_EQ(after_inline_flex,
            LocalCaretRectOf(Position::LastPositionInNode(target)));
}

TEST_P(ParameterizedLocalCaretRectTest, SimpleText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>XXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, MixedHeightText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>Xpp</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, RtlText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl id=bdo style='display: block; "
      "font: 10px/10px Ahem; width: 30px'>XXX</bdo>");
  const Node* foo = GetElementById("bdo")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, ClampingAndRounding) {
  // crbug.com/1228620
  LoadAhem();
  SetBodyContent(R"HTML(
      <style>
      #root {
        margin-left: 0.6px;
        width: 150.6px;
        text-align: right;
        font: 30px/30px Ahem;
      }
      </style>
      <div id=root>def</div>)HTML");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(149, 0, 1, 30)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, OverflowTextLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; width: 30px'>"
      "XXXX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(39, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 4), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, UnderflowTextLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; width: 30px'>"
      "XX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, OverflowTextRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; width: 30px' "
      "dir=rtl>"
      "XXXX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(-10, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text, 4), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, UnderflowTextRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; width: 30px' "
      "dir=rtl>"
      "XX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, VerticalRLText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='writing-mode: vertical-rl; word-break: break-all; "
      "font: 10px/10px Ahem; width: 30px; height: 30px'>XXXYYYZZZ</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 7), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 8), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 9), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, VerticalLRText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='writing-mode: vertical-lr; word-break: break-all; "
      "font: 10px/10px Ahem; width: 30px; height: 30px'>XXXYYYZZZ</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 7), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 8), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 9), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, OverflowTextVerticalLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; height: 30px; writing-mode: "
      "vertical-lr'>"
      "XXXX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 39, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 4), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, UnderflowTextVerticalLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; height: 30px; writing-mode: "
      "vertical-lr'>"
      "XX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, OverflowTextVerticalRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; height: 30px; "
      "writing-mode: vertical-lr' dir=rtl>"
      "XXXX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, -10, 10, 1)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text, 4), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, UnderflowTextVerticalRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; height: 30px; "
      "writing-mode: vertical-lr' dir=rtl>"
      "XX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, TwoLinesOfTextWithSoftWrap) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px; "
      "word-break: break-all'>XXXXXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  // First line
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  // Second line
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, SoftLineWrapBetweenMultipleTextNodes) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width: 30px; word-break: break-all'>"
      "<span>A</span>"
      "<span>B</span>"
      "<span id=span-c>C</span>"
      "<span id=span-d>D</span>"
      "<span>E</span>"
      "<span>F</span>"
      "</div>");
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();

  const Position after_c(text_c, 1);
  EXPECT_EQ(
      LocalCaretRect(text_c->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kDownstream)));

  const Position before_d(text_d, 0);
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest,
       SoftLineWrapBetweenMultipleTextNodesRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl style='font: 10px/10px Ahem; width: 30px; "
      "word-break: break-all; display: block'>"
      "<span>A</span>"
      "<span>B</span>"
      "<span id=span-c>C</span>"
      "<span id=span-d>D</span>"
      "<span>E</span>"
      "<span>F</span>"
      "</bdo>");
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();

  const Position after_c(text_c, 1);
  EXPECT_EQ(
      LocalCaretRect(text_c->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kDownstream)));

  const Position before_d(text_d, 0);
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, CaretRectAtBR) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width: 30px'><br>foo</div>");
  const Element& br = *GetDocument().QuerySelector("br");

  EXPECT_EQ(LocalCaretRect(br.GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(br), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, CaretRectAtRtlBR) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl style='display: block; font: 10px/10px Ahem; width: 30px'>"
      "<br>foo</bdo>");
  const Element& br = *GetDocument().QuerySelector("br");

  EXPECT_EQ(LocalCaretRect(br.GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(br), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, Images) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>"
      "<img id=img1 width=10px height=10px>"
      "<img id=img2 width=10px height=10px>"
      "</div>");

  const Element& img1 = *GetElementById("img1");

  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(0, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img1), TextAffinity::kDownstream)));

  const Element& img2 = *GetElementById("img2");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(9, 0, 1, 12))
          : LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(0, 0, 1, 12)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::BeforeNode(img2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, RtlImages) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl style='font: 10px/10px Ahem; width: 30px; display: block'>"
      "<img id=img1 width=10px height=10px>"
      "<img id=img2 width=10px height=10px>"
      "</bdo>");

  const Element& img1 = *GetElementById("img1");
  const Element& img2 = *GetElementById("img2");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img1), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(9, 0, 1, 12))
          : LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(0, 0, 1, 12)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(img1), TextAffinity::kDownstream)));

  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(0, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, VerticalImage) {
  // This test only records the current behavior. Future changes are allowed.

  SetBodyContent(
      "<div style='writing-mode: vertical-rl'>"
      "<img id=img width=10px height=20px>"
      "</div>");

  const Element& img = *GetElementById("img");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img), TextAffinity::kDownstream)));

  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img.GetLayoutObject(), PhysicalRect(0, 19, 10, 1))
          // TODO(crbug.com/805064): The legacy behavior is wrong. Fix it.
          : LocalCaretRect(img.GetLayoutObject(), PhysicalRect(0, 9, 10, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(img), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, TextAndImageMixedHeight) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>"
      "X"
      "<img id=img width=10px height=5px style='vertical-align: text-bottom'>"
      "p</div>");

  const Element& img = *GetElementById("img");
  const Node* text1 = img.previousSibling();
  const Node* text2 = img.nextSibling();

  EXPECT_EQ(LocalCaretRect(text1->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text1, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(text1->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text1, 1), TextAffinity::kDownstream)));

  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(text1->GetLayoutObject(), PhysicalRect(10, 0, 1, 10))
          : LocalCaretRect(img.GetLayoutObject(), PhysicalRect(0, -5, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::BeforeNode(img), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), PhysicalRect(9, -5, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img), TextAffinity::kDownstream)));

  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(text2->GetLayoutObject(),
                                               PhysicalRect(20, 0, 1, 10))
                              : LocalCaretRect(text2->GetLayoutObject(),
                                               PhysicalRect(20, 5, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text2, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(text2->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text2, 1), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, FloatFirstLetter) {
  LoadAhem();
  InsertStyleElement("#container::first-letter{float:right}");
  SetBodyContent(
      "<div id=container style='font: 10px/10px Ahem; width: 40px'>foo</div>");
  const Node* foo = GetElementById("container")->firstChild();
  const LayoutObject* first_letter = AssociatedLayoutObjectOf(*foo, 0);
  const LayoutObject* remaining_text = AssociatedLayoutObjectOf(*foo, 1);

  // TODO(editing-dev): Legacy LocalCaretRectOfPosition() is not aware of the
  // first-letter LayoutObject. Fix it.

  EXPECT_EQ(LocalCaretRect(LayoutNGEnabled() ? first_letter : remaining_text,
                           PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text,
                           PhysicalRect(LayoutNGEnabled() ? 0 : 10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text,
                           PhysicalRect(LayoutNGEnabled() ? 10 : 20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text, PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreak) {
  LoadAhem();
  SetBodyContent("<div style='font: 10px/10px Ahem;'>foo<br><br></div>");
  const Node* div = GetDocument().body()->firstChild();
  const Node* foo = div->firstChild();
  const Node* first_br = foo->nextSibling();
  const Node* second_br = first_br->nextSibling();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*foo), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(second_br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(*first_br), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(second_br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(*second_br), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPre) {
  LoadAhem();
  SetBodyContent("<pre style='font: 10px/10px Ahem;'>foo\n\n</pre>");
  const Node* pre = GetDocument().body()->firstChild();
  const Node* foo = pre->firstChild();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPre2) {
  LoadAhem();
  // This test case simulates the rendering of the inner editor of
  // <textarea>foo\n</textarea> without using text control element.
  SetBodyContent("<pre style='font: 10px/10px Ahem;'>foo\n<br></pre>");
  const Node* pre = GetDocument().body()->firstChild();
  const Node* foo = pre->firstChild();
  const Node* br = foo->nextSibling();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*br), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakTextArea) {
  LoadAhem();
  SetBodyContent("<textarea style='font: 10px/10px Ahem; '>foo\n\n</textarea>");
  const auto* textarea = ToTextControl(GetDocument().body()->firstChild());
  const Node* inner_text = textarea->InnerEditorElement()->firstChild();
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 4), TextAffinity::kDownstream)));
  const Node* hidden_br = inner_text->nextSibling();
  EXPECT_EQ(
      LocalCaretRect(hidden_br->GetLayoutObject(), PhysicalRect(0, 20, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 5), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, CollapsedSpace) {
  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem;'>"
      "<span>foo</span><span>  </span></div>");
  const Node* first_span = GetDocument().body()->firstChild()->firstChild();
  const Node* foo = first_span->firstChild();
  const Node* second_span = first_span->nextSibling();
  const Node* white_spaces = second_span->firstChild();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*foo), TextAffinity::kDownstream)));
  // TODO(yoichio): Following should return valid rect: crbug.com/812535.
  EXPECT_EQ(
      LocalCaretRect(first_span->GetLayoutObject(), PhysicalRect(0, 0, 0, 0)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position::LastPositionInNode(*first_span),
                               TextAffinity::kDownstream)));
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(foo->GetLayoutObject(),
                                               PhysicalRect(30, 0, 1, 10))
                              : LocalCaretRect(white_spaces->GetLayoutObject(),
                                               PhysicalRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(foo->GetLayoutObject(),
                                               PhysicalRect(30, 0, 1, 10))
                              : LocalCaretRect(white_spaces->GetLayoutObject(),
                                               PhysicalRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(foo->GetLayoutObject(),
                                               PhysicalRect(30, 0, 1, 10))
                              : LocalCaretRect(white_spaces->GetLayoutObject(),
                                               PhysicalRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AbsoluteCaretBoundsOfWithShadowDOM) {
  const char* body_content =
      "<p id='host'><b slot='#one' id='one'>11</b><b name='#two' "
      "id='two'>22</b></p>";
  const char* shadow_content =
      "<div><slot name=#two></slot><slot name=#one></slot></div>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector("#one");

  gfx::Rect bounds_in_dom_tree = AbsoluteCaretBoundsOf(
      CreateVisiblePosition(Position(one, 0)).ToPositionWithAffinity());
  gfx::Rect bounds_in_flat_tree =
      AbsoluteCaretBoundsOf(CreateVisiblePosition(PositionInFlatTree(one, 0))
                                .ToPositionWithAffinity());

  EXPECT_FALSE(bounds_in_dom_tree.IsEmpty());
  EXPECT_EQ(bounds_in_dom_tree, bounds_in_flat_tree);
}

// Repro case of crbug.com/680428
TEST_P(ParameterizedLocalCaretRectTest, AbsoluteSelectionBoundsOfWithImage) {
  SetBodyContent("<div>foo<img></div>");

  Node* node = GetDocument().QuerySelector("img");
  gfx::Rect rect = AbsoluteSelectionBoundsOf(VisiblePosition::Create(
      PositionWithAffinity(Position::LastPositionInNode(*node))));
  EXPECT_FALSE(rect.IsEmpty());
}

static std::pair<PhysicalRect, PhysicalRect> GetPhysicalRects(
    const Position& caret) {
  const PositionWithAffinity position(caret);
  const PhysicalRect& position_rect = LocalCaretRectOfPosition(position).rect;
  const PositionWithAffinity visible_position(
      CreateVisiblePosition(position).DeepEquivalent());
  const PhysicalRect& visible_position_rect =
      LocalCaretRectOfPosition(visible_position).rect;
  return {position_rect, visible_position_rect};
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPreBlockLTRLineLTR) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='ltr'>foo\n|<bdo dir='ltr'>abc</bdo></pre>");
  PhysicalRect position_rect, visible_position_rect;
  std::tie(position_rect, visible_position_rect) = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), visible_position_rect);
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPreBlockLTRLineRTL) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='ltr'>foo\n|<bdo dir='rtl'>abc</bdo></pre>");
  PhysicalRect position_rect, visible_position_rect;
  std::tie(position_rect, visible_position_rect) = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), visible_position_rect);
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPreBlockRTLLineLTR) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='rtl'>foo\n|<bdo dir='ltr'>abc</bdo></pre>");
  PhysicalRect position_rect, visible_position_rect;
  std::tie(position_rect, visible_position_rect) = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), visible_position_rect);
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPreBlockRTLLineRTL) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='rtl'>foo\n|<bdo dir='rtl'>abc</bdo></pre>");
  PhysicalRect position_rect, visible_position_rect;
  std::tie(position_rect, visible_position_rect) = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), visible_position_rect);
}

// crbug.com/834686
TEST_P(ParameterizedLocalCaretRectTest, AfterTrimedLineBreak) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/10px Ahem; width: 300px }");
  const Position& caret = SetCaretTextToBody("<div>foo\n|</div>");
  PhysicalRect position_rect, visible_position_rect;
  std::tie(position_rect, visible_position_rect) = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10), visible_position_rect);
}

// See also NGCaretPositionTest.MultiColumnSingleText
TEST_P(ParameterizedLocalCaretRectTest, MultiColumnSingleText) {
  RuntimeEnabledFeaturesTestHelpers::ScopedLayoutNGBlockFragmentation
      block_fragmentation(LayoutNGEnabled());
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/15px Ahem; column-count: 3; width: 20ch; }");
  SetBodyInnerHTML("<div id=target>abc def ghi jkl mno pqr</div>");
  // This HTML is rendered as:
  //    abc ghi mno
  //    def jkl
  const auto& target = *GetElementById("target");
  const Text& text = *To<Text>(target.firstChild());
  const bool block_fragmentation_enabled =
      RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled();

  // Note: Legacy layout caret rect is in stitch coordinate space == as if
  // columns are laid out vertically.
  // NG caret rect is in relative to containing box fragment.

  // "abc " in column 1
  EXPECT_EQ(PhysicalRect(0, 2, 1, 10),
            LocalCaretRectOf(Position(text, 0)).rect);
  EXPECT_EQ(PhysicalRect(10, 2, 1, 10),
            LocalCaretRectOf(Position(text, 1)).rect);
  EXPECT_EQ(PhysicalRect(20, 2, 1, 10),
            LocalCaretRectOf(Position(text, 2)).rect);
  EXPECT_EQ(PhysicalRect(30, 2, 1, 10),
            LocalCaretRectOf(Position(text, 3)).rect);

  // "def " in column 1
  EXPECT_EQ(PhysicalRect(0, 17, 1, 10),
            LocalCaretRectOf(Position(text, 4)).rect);
  EXPECT_EQ(PhysicalRect(10, 17, 1, 10),
            LocalCaretRectOf(Position(text, 5)).rect);
  EXPECT_EQ(PhysicalRect(20, 17, 1, 10),
            LocalCaretRectOf(Position(text, 6)).rect);
  EXPECT_EQ(PhysicalRect(30, 17, 1, 10),
            LocalCaretRectOf(Position(text, 7)).rect);

  // "ghi " in column 2
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(0, 2, 1, 10)
                                        : PhysicalRect(0, 32, 1, 10),
            LocalCaretRectOf(Position(text, 8)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(10, 2, 1, 10)
                                        : PhysicalRect(10, 32, 1, 10),
            LocalCaretRectOf(Position(text, 9)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(20, 2, 1, 10)
                                        : PhysicalRect(20, 32, 1, 10),
            LocalCaretRectOf(Position(text, 10)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(30, 2, 1, 10)
                                        : PhysicalRect(30, 32, 1, 10),
            LocalCaretRectOf(Position(text, 11)).rect);

  // "jkl " in column 2
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(0, 17, 1, 10)
                                        : PhysicalRect(0, 47, 1, 10),
            LocalCaretRectOf(Position(text, 12)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(10, 17, 1, 10)
                                        : PhysicalRect(10, 47, 1, 10),
            LocalCaretRectOf(Position(text, 13)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(20, 17, 1, 10)
                                        : PhysicalRect(20, 47, 1, 10),
            LocalCaretRectOf(Position(text, 14)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(30, 17, 1, 10)
                                        : PhysicalRect(30, 47, 1, 10),
            LocalCaretRectOf(Position(text, 15)).rect);

  // "mno " in column 3
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(0, 2, 1, 10)
                                        : PhysicalRect(0, 62, 1, 10),
            LocalCaretRectOf(Position(text, 16)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(10, 2, 1, 10)
                                        : PhysicalRect(10, 62, 1, 10),
            LocalCaretRectOf(Position(text, 17)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(20, 2, 1, 10)
                                        : PhysicalRect(20, 62, 1, 10),
            LocalCaretRectOf(Position(text, 18)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(30, 2, 1, 10)
                                        : PhysicalRect(30, 62, 1, 10),
            LocalCaretRectOf(Position(text, 19)).rect);

  // "pqr" in column 3
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(0, 17, 1, 10)
                                        : PhysicalRect(0, 77, 1, 10),
            LocalCaretRectOf(Position(text, 20)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(10, 17, 1, 10)
                                        : PhysicalRect(10, 77, 1, 10),
            LocalCaretRectOf(Position(text, 21)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(20, 17, 1, 10)
                                        : PhysicalRect(20, 77, 1, 10),
            LocalCaretRectOf(Position(text, 22)).rect);
  EXPECT_EQ(block_fragmentation_enabled ? PhysicalRect(30, 17, 1, 10)
                                        : PhysicalRect(30, 77, 1, 10),
            LocalCaretRectOf(Position(text, 23)).rect);
}

TEST_P(ParameterizedLocalCaretRectTest,
       UnicodeBidiPlaintextWithDifferentBlockDirection) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/10px Ahem; unicode-bidi: plaintext }");
  const Position position = SetCaretTextToBody("<div dir='rtl'>|abc</div>");
  const PhysicalRect caret_rect =
      LocalCaretRectOfPosition(PositionWithAffinity(position)).rect;
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10), caret_rect);
}

// http://crbug.com/835779
TEST_P(ParameterizedLocalCaretRectTest, NextLineWithoutLeafChild) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/10px Ahem; width: 30px }");
  SetBodyContent(
      "<div>"
      "<br>"
      "<span style=\"border-left: 50px solid\"></span>"
      "foo"
      "</div>");

  const Element& br = *GetDocument().QuerySelector("br");
  EXPECT_EQ(
      // TODO(xiaochengh): Should return the same result for legacy and
      // LayoutNG.
      LayoutNGEnabled() ? PhysicalRect(50, 10, 1, 10)
                        : PhysicalRect(0, 20, 1, 10),
      LocalCaretRectOfPosition(PositionWithAffinity(Position::AfterNode(br)))
          .rect);
}

TEST_P(ParameterizedLocalCaretRectTest, BidiTextWithImage) {
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/10px Ahem; width: 30px }"
      "img { width: 10px; height: 10px; vertical-align: bottom }");
  SetBodyContent("<div dir=rtl>X<img id=image>Y</div>");
  const Element& image = *GetElementById("image");
  const LayoutObject* image_layout = image.GetLayoutObject();
  const LayoutObject* text_before = image.previousSibling()->GetLayoutObject();
  // TODO(xiaochengh): Should return the same result for legacy and NG
  EXPECT_EQ(LayoutNGEnabled()
                ? LocalCaretRect(text_before, PhysicalRect(10, 0, 1, 10))
                : LocalCaretRect(image_layout, PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(Position::BeforeNode(image))));
  EXPECT_EQ(LocalCaretRect(image_layout, PhysicalRect(9, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(Position::AfterNode(image))));
}

// https://crbug.com/876044
TEST_P(ParameterizedLocalCaretRectTest, RtlMeterNoCrash) {
  SetBodyContent("foo<meter dir=rtl></meter>");
  const Position position = Position::LastPositionInNode(*GetDocument().body());
  // Shouldn't crash inside
  const LocalCaretRect local_caret_rect =
      LocalCaretRectOfPosition(PositionWithAffinity(position));
  EXPECT_EQ(GetDocument().QuerySelector("meter")->GetLayoutObject(),
            local_caret_rect.layout_object);
}

// https://crbug.com/883044
TEST_P(ParameterizedLocalCaretRectTest, AfterCollapsedWhiteSpaceInRTLText) {
  LoadAhem();
  InsertStyleElement(
      "bdo { display: block; font: 10px/10px Ahem; width: 100px }");
  const Position position =
      SetCaretTextToBody("<bdo dir=rtl>AAA  |BBB<span>CCC</span></bdo>");
  const Node* text = position.AnchorNode();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(60, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(position, TextAffinity::kDownstream)));
}

// https://crbug.com/936988
TEST_P(ParameterizedLocalCaretRectTest, AfterIneditableInline) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement("div { font: 10px/10px Ahem }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");
  const Element* div = GetDocument().QuerySelector("div");
  const Node* text = div->firstChild()->firstChild();

  const Position position = Position::LastPositionInNode(*div);
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(position)));
}

// http://crbug.com/688015
TEST_P(ParameterizedLocalCaretRectTest, LocalCaretAtBeginningOfNonEditable) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");
  const Element* div = GetDocument().QuerySelector("div");
  const Node* text = div->firstChild()->firstChild();

  const Position& position = Position::FirstPositionInNode(*div);
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(LocalCaretRect(div->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_P(ParameterizedLocalCaretRectTest,
       LocalCaretAtBeginningOfNonEditableInFlatTree) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  const char* body_content =
      "<div contenteditable><div id='host'>"
      "<span slot='#one'>foo1</span><span>foo3</span></div></div>";
  const char* shadow_content =
      "<slot name=#one></slot>"
      "<span contenteditable='false' id='foo2'>foo2</span>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  const Element* target = GetDocument().getElementById("host");
  const Node* foo1 = target->firstChild()->firstChild();

  const Position& position = Position::FirstPositionInNode(*target);
  EXPECT_EQ(LocalCaretRect(foo1->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));

  const PositionInFlatTree& position_in_flat_tree =
      PositionInFlatTree::FirstPositionInNode(*target);
  EXPECT_EQ(LocalCaretRect(foo1->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionInFlatTreeWithAffinity(position_in_flat_tree),
                kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionInFlatTreeWithAffinity(position_in_flat_tree),
          kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_P(ParameterizedLocalCaretRectTest, LocalCaretAtEndOfNonEditable) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");
  const Element* div = GetDocument().QuerySelector("div");
  const Node* text = div->firstChild()->firstChild();

  const Position& position = Position::LastPositionInNode(*div);
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(55, 10, 1, 10)),
      LocalCaretRectOf(position, kCanCrossEditingBoundary));
  // TODO(jfernandez): It should be 89, but LayoutBox::LocalCaretRect is buggy
  // and it adds the padding-left twice.
  // TODO(jfernandez): We might avoid using LayoutBox::LocalCaretRect when using
  // LayoutNG
  EXPECT_EQ(LocalCaretRect(div->GetLayoutObject(), PhysicalRect(99, 10, 1, 10)),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_P(ParameterizedLocalCaretRectTest,
       LocalCaretAtEndOfNonEditableInFlatTree) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px }");
  const char* body_content =
      "<div contenteditable><div id='host'>"
      "<span slot='#one'>foo1</span><span>foo3</span></div></div>";
  const char* shadow_content =
      "<slot name=#one></slot>"
      "<span contenteditable='false' id='foo2'>foo2</span>";
  SetBodyContent(body_content);
  auto* shadow_root = SetShadowContent(shadow_content, "host");
  const Element* target = GetDocument().getElementById("host");
  const Node* foo2 = shadow_root->getElementById("foo2")->firstChild();

  // TODO(jfernandez): It should be 89, but LayoutBox::LocalCaretRect is buggy
  // and it adds the padding-left twice.
  // TODO(jfernandez): As a matter of fact, 69 would be better result IMHO,
  // positioning the caret at the end of the non-editable area.
  // TODO(jfernandez): We might avoid using LayoutBox::LocalCaretRect when using
  // LayoutNG
  const Position& position = Position::LastPositionInNode(*target);
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(99, 10, 1, 10)),
      LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(99, 10, 1, 10)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));

  const PositionInFlatTree& position_in_flat_tree =
      PositionInFlatTree::LastPositionInNode(*target);
  EXPECT_EQ(
      LocalCaretRect(foo2->GetLayoutObject(), PhysicalRect(50, 20, 1, 10)),
      LocalCaretRectOfPosition(
          PositionInFlatTreeWithAffinity(position_in_flat_tree),
          kCanCrossEditingBoundary));
  // TODO(jfernandez): It should be 89, but LayoutBox::LocalCaretRect is buggy
  // and it adds the padding-left twice.
  // TODO(jfernandez): We might avoid using LayoutBox::LocalCaretRect when using
  // LayoutNG
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(99, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionInFlatTreeWithAffinity(position_in_flat_tree),
          kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_P(ParameterizedLocalCaretRectTest, AbsoluteCaretAtEndOfNonEditable) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement(
      "body { margin: 5px; }"
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");

  const Element* div = GetDocument().QuerySelector("div");
  const Position& position = Position::LastPositionInNode(*div);
  EXPECT_EQ("60,5 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(position), nullptr,
                                  kCanCrossEditingBoundary)
                .ToString());
  // TODO(jfernandez): It should be 84, but LayoutBox::LocalCaretRect is buggy
  // and it adds the padding-left twice.
  // TODO(jfernandez): As a matter of fact, 69 would be better result IMHO,
  // positioning the caret at the end of the non-editable area.
  // TODO(jfernandez): We might avoid using LayoutBox::LocalCaretRect when using
  // LayoutNG
  EXPECT_EQ("94,5 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(position), nullptr,
                                  kCannotCrossEditingBoundary)
                .ToString());
}

// http://crbug.com/688015
TEST_P(ParameterizedLocalCaretRectTest, AbsoluteCaretAtBeginningOfNonEditable) {
  // For LayoutNG, we also enable EditingNG to test NG caret rendering.
  ScopedEditingNGForTest editing_ng(LayoutNGEnabled());

  LoadAhem();
  InsertStyleElement(
      "body { margin: 5px; }"
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");

  const Element* div = GetDocument().QuerySelector("div");
  const Position& position = Position::FirstPositionInNode(*div);
  EXPECT_EQ("30,5 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(position), nullptr,
                                  kCanCrossEditingBoundary)
                .ToString());
  EXPECT_EQ("15,5 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(position), nullptr,
                                  kCannotCrossEditingBoundary)
                .ToString());
}

// https://crbug.com/1155399
TEST_P(ParameterizedLocalCaretRectTest, OptionWithDisplayContents) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/10px Ahem; width: 300px }"
      "option { display: contents; }");
  SetBodyContent("<option>a</option>");
  const Element* body = GetDocument().body();
  const Element* option = GetDocument().QuerySelector("option");
  LocalCaretRect empty;
  LocalCaretRect start(body->GetLayoutObject(), PhysicalRect(0, 0, 1, 10));
  LocalCaretRect end(body->GetLayoutObject(), PhysicalRect(299, 0, 1, 10));

  // LocalCaretRectOfPosition shouldn't crash
  for (const Position& p : {Position::BeforeNode(*body), Position(body, 0)})
    EXPECT_EQ(start, LocalCaretRectOfPosition(PositionWithAffinity(p)));
  for (const Position& p :
       {Position::BeforeNode(*option), Position(option, 0), Position(option, 1),
        Position::LastPositionInNode(*option), Position::AfterNode(*option)})
    EXPECT_EQ(empty, LocalCaretRectOfPosition(PositionWithAffinity(p)));
  for (const Position& p :
       {Position(body, 1), Position::LastPositionInNode(*body),
        Position::AfterNode(*body)})
    EXPECT_EQ(end, LocalCaretRectOfPosition(PositionWithAffinity(p)));
}

TEST_P(ParameterizedLocalCaretRectTest, TextCombineOneTextNode) {
  if (!LayoutNGEnabled())
    return;
  ScopedLayoutNGTextCombineForTest enable_layout_ng_text_combine(true);
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  font: 100px/110px Ahem;"
      "  writing-mode: vertical-rl;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div>a<tcy id=target>01234</tcy>b</div>");
  //   LayoutNGBlockFlow {HTML} at (0,0) size 800x600
  //     LayoutNGBlockFlow {BODY} at (8,8) size 784x584
  //       LayoutNGBlockFlow {DIV} at (0,0) size 110x300
  //         LayoutText {#text} at (5,0) size 100x100
  //           text run at (5,0) width 100: "a"
  //         LayoutInline {TCY} at (5,100) size 100x100
  //           LayoutNGTextCombine (anonymous) at (5,100) size 100x100
  //             LayoutText {#text} at (-5,0) size 110x100
  //               text run at (0,0) width 500: "01234"
  //         LayoutText {#text} at (5,200) size 100x100
  //           text run at (5,200) width 100: "b"
  const auto& target = *GetElementById("target");
  const auto& text_a = *To<Text>(target.previousSibling());
  const auto& text_01234 = *To<Text>(target.firstChild());
  const auto& text_b = *To<Text>(target.nextSibling());

  // text_a
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 0, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 100, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 1))));

  // text_01234
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(0, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(17, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 1))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(39, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 2))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(61, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 3))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(83, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 4))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(99, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 5))));

  // text_b
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 200, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 299, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 1))));
}

TEST_P(ParameterizedLocalCaretRectTest, TextCombineTwoTextNodes) {
  if (!LayoutNGEnabled())
    return;
  ScopedLayoutNGTextCombineForTest enable_layout_ng_text_combine(true);
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  font: 100px/110px Ahem;"
      "  writing-mode: vertical-rl;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div>a<tcy id=target>012<!-- -->34</tcy>b</div>");
  //   LayoutNGBlockFlow {HTML} at (0,0) size 800x600
  //     LayoutNGBlockFlow {BODY} at (8,8) size 784x584
  //       LayoutNGBlockFlow {DIV} at (0,0) size 110x300
  //         LayoutText {#text} at (5,0) size 100x100
  //           text run at (5,0) width 100: "a"
  //         LayoutInline {TCY} at (5,100) size 100x100
  //           LayoutNGTextCombine (anonymous) at (5,100) size 100x100
  //             LayoutText {#text} at (-5,0) size 66x100
  //               text run at (0,0) width 300: "012"
  //             LayoutText {#text} at (61,0) size 44x100
  //               text run at (300,0) width 200: "34"
  //         LayoutText {#text} at (5,200) size 100x100
  //           text run at (5,200) width 100: "b"
  const auto& target = *GetElementById("target");
  const auto& text_a = *To<Text>(target.previousSibling());
  const auto& text_012 = *To<Text>(target.firstChild());
  const auto& text_34 = *To<Text>(target.lastChild());
  const auto& text_b = *To<Text>(target.nextSibling());

  // text_a
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 0, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 100, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 1))));

  // text_012
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(0, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(17, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 1))));
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(39, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 2))));
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(61, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 3))));

  // text_34
  EXPECT_EQ(
      LocalCaretRect(text_34.GetLayoutObject(), PhysicalRect(61, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_34, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_34.GetLayoutObject(), PhysicalRect(83, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_34, 1))));
  EXPECT_EQ(
      LocalCaretRect(text_34.GetLayoutObject(), PhysicalRect(99, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_34, 2))));

  // text_b
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 200, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 299, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 1))));
}

}  // namespace blink
