// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/LocalCaretRect.h"

#include "core/editing/PositionWithAffinity.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/html/forms/TextControlElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "platform/testing/runtime_enabled_features_test_helpers.h"

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
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(All, ParameterizedLocalCaretRectTest, testing::Bool());

TEST_P(ParameterizedLocalCaretRectTest, DOMAndFlatTrees) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
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

TEST_P(ParameterizedLocalCaretRectTest, SimpleText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>XXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, MixedHeightText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>Xpp</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
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

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, OverflowTextLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; width: 30px'>"
      "XXXX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(39, 0, 1, 10)),
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(20, 0, 1, 10)),
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(-10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 4), TextAffinity::kDownstream)));
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

// TODO(xiaochengh): Fix NG LocalCaretText computation for vertical text.
TEST_F(LocalCaretRectTest, VerticalRLText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='writing-mode: vertical-rl; word-break: break-all; "
      "font: 10px/10px Ahem; width: 30px; height: 30px'>XXXYYYZZZ</div>");
  const Node* foo = GetElementById("div")->firstChild();

  // TODO(xiaochengh): In vertical-rl writing mode, the |X| property of
  // LocalCaretRect's LayoutRect seems to refer to the distance to the right
  // edge of the inline formatting context instead. Figure out if this is
  // correct.

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 7), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 8), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 29, 10, 1)),
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

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 7), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 8), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 29, 10, 1)),
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 39, 10, 1)),
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 20, 10, 1)),
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, -10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 4), TextAffinity::kDownstream)));
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
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), LayoutRect(0, 10, 10, 1)),
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
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  // Second line
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(10, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(20, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(29, 10, 1, 10)),
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
  EXPECT_EQ(LocalCaretRect(text_c->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(after_c, TextAffinity::kUpstream)));
  EXPECT_EQ(LocalCaretRect(text_d->GetLayoutObject(), LayoutRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(after_c, TextAffinity::kDownstream)));

  const Position before_d(text_d, 0);
  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(text_c->GetLayoutObject(), LayoutRect(29, 0, 1, 10))
          : LocalCaretRect(text_d->GetLayoutObject(), LayoutRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kUpstream)));
  EXPECT_EQ(LocalCaretRect(text_d->GetLayoutObject(), LayoutRect(0, 10, 1, 10)),
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
  EXPECT_EQ(LocalCaretRect(text_c->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(after_c, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), LayoutRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kDownstream)));

  const Position before_d(text_d, 0);
  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(text_c->GetLayoutObject(),
                                               LayoutRect(0, 0, 1, 10))
                              : LocalCaretRect(text_d->GetLayoutObject(),
                                               LayoutRect(29, 10, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(before_d, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), LayoutRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, CaretRectAtBR) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width: 30px'><br>foo</div>");
  const Element& br = *GetDocument().QuerySelector("br");

  EXPECT_EQ(LocalCaretRect(br.GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
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

  EXPECT_EQ(LocalCaretRect(br.GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
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

  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), LayoutRect(0, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), LayoutRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img1), TextAffinity::kDownstream)));

  const Element& img2 = *GetElementById("img2");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img1.GetLayoutObject(), LayoutRect(9, 0, 1, 12))
          : LocalCaretRect(img2.GetLayoutObject(), LayoutRect(0, 0, 1, 12)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::BeforeNode(img2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), LayoutRect(9, 0, 1, 12)),
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
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), LayoutRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img1), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img2.GetLayoutObject(), LayoutRect(9, 0, 1, 12))
          : LocalCaretRect(img1.GetLayoutObject(), LayoutRect(0, 0, 1, 12)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(img1), TextAffinity::kDownstream)));

  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), LayoutRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), LayoutRect(0, 0, 1, 12)),
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
  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), LayoutRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img), TextAffinity::kDownstream)));

  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img.GetLayoutObject(), LayoutRect(0, 19, 10, 1))
          // TODO(crbug.com/805064): The legacy behavior is wrong. Fix it.
          : LocalCaretRect(img.GetLayoutObject(), LayoutRect(0, 9, 10, 1)),
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

  EXPECT_EQ(LocalCaretRect(text1->GetLayoutObject(), LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text1, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(text1->GetLayoutObject(), LayoutRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text1, 1), TextAffinity::kDownstream)));

  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(text1->GetLayoutObject(), LayoutRect(10, 0, 1, 10))
          : LocalCaretRect(img.GetLayoutObject(), LayoutRect(0, -5, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::BeforeNode(img), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), LayoutRect(9, -5, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img), TextAffinity::kDownstream)));

  // TODO(xiaochengh): Should return the same result for legacy and LayoutNG.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(img.GetLayoutObject(), LayoutRect(9, -5, 1, 10))
          : LocalCaretRect(text2->GetLayoutObject(), LayoutRect(20, 5, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(text2->GetLayoutObject(), LayoutRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text2, 1), TextAffinity::kDownstream)));
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
                           LayoutRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text,
                           LayoutRect(LayoutNGEnabled() ? 0 : 10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text,
                           LayoutRect(LayoutNGEnabled() ? 10 : 20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text, LayoutNGEnabled()
                                               ? LayoutRect(20, 0, 1, 10)
                                               : LayoutRect()),
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
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*foo), TextAffinity::kDownstream)));
  // TODO(yoichio): Legacy should return valid rect: crbug.com/812535.
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(second_br->GetLayoutObject(),
                                               LayoutRect(0, 10, 1, 10))
                              : LocalCaretRect(first_br->GetLayoutObject(),
                                               LayoutRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*first_br), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(second_br->GetLayoutObject(),
                           LayoutNGEnabled() ? LayoutRect(0, 10, 1, 10)
                                             : LayoutRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*second_br), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakInPre) {
  LoadAhem();
  SetBodyContent("<pre style='font: 10px/10px Ahem;'>foo\n\n</pre>");
  const Node* pre = GetDocument().body()->firstChild();
  const Node* foo = pre->firstChild();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  // TODO(yoichio): Legacy should return valid rect: crbug.com/812535.
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(),
                           LayoutNGEnabled() ? LayoutRect(0, 10, 1, 10)
                                             : LayoutRect(0, 0, 0, 0)),
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
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  // TODO(yoichio): Legacy should return valid rect: crbug.com/812535.
  EXPECT_EQ(
      LayoutNGEnabled()
          ? LocalCaretRect(br->GetLayoutObject(), LayoutRect(0, 10, 1, 10))
          : LocalCaretRect(foo->GetLayoutObject(), LayoutRect(0, 0, 0, 0)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(br->GetLayoutObject(), LayoutNGEnabled()
                                                      ? LayoutRect(0, 10, 1, 10)
                                                      : LayoutRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*br), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AfterLineBreakTextArea) {
  LoadAhem();
  SetBodyContent("<textarea style='font: 10px/10px Ahem; '>foo\n\n</textarea>");
  const TextControlElement* textarea =
      ToTextControlElement(GetDocument().body()->firstChild());
  const Node* inner_text = textarea->InnerEditorElement()->firstChild();
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), LayoutRect(30, 0, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), LayoutRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 4), TextAffinity::kDownstream)));
  // TODO(yoichio): Following should return valid rect: crbug.com/812535.
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), LayoutRect(0, 0, 0, 0)),
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
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), LayoutRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*foo), TextAffinity::kDownstream)));
  // TODO(yoichio): Following should return valid rect: crbug.com/812535.
  EXPECT_EQ(
      LocalCaretRect(first_span->GetLayoutObject(), LayoutRect(0, 0, 0, 0)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(first_span, PositionAnchorType::kAfterChildren),
          TextAffinity::kDownstream)));
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(foo->GetLayoutObject(),
                                               LayoutRect(30, 0, 1, 10))
                              : LocalCaretRect(white_spaces->GetLayoutObject(),
                                               LayoutRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(foo->GetLayoutObject(),
                                               LayoutRect(30, 0, 1, 10))
                              : LocalCaretRect(white_spaces->GetLayoutObject(),
                                               LayoutRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LayoutNGEnabled() ? LocalCaretRect(foo->GetLayoutObject(),
                                               LayoutRect(30, 0, 1, 10))
                              : LocalCaretRect(white_spaces->GetLayoutObject(),
                                               LayoutRect(0, 0, 0, 0)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 2), TextAffinity::kDownstream)));
}

TEST_P(ParameterizedLocalCaretRectTest, AbsoluteCaretBoundsOfWithShadowDOM) {
  const char* body_content =
      "<p id='host'><b id='one'>11</b><b id='two'>22</b></p>";
  const char* shadow_content =
      "<div><content select=#two></content><content "
      "select=#one></content></div>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector("#one");

  IntRect bounds_in_dom_tree =
      AbsoluteCaretBoundsOf(CreateVisiblePosition(Position(one, 0)));
  IntRect bounds_in_flat_tree =
      AbsoluteCaretBoundsOf(CreateVisiblePosition(PositionInFlatTree(one, 0)));

  EXPECT_FALSE(bounds_in_dom_tree.IsEmpty());
  EXPECT_EQ(bounds_in_dom_tree, bounds_in_flat_tree);
}

// Repro case of crbug.com/680428
TEST_P(ParameterizedLocalCaretRectTest, AbsoluteSelectionBoundsOfWithImage) {
  SetBodyContent("<div>foo<img></div>");

  Node* node = GetDocument().QuerySelector("img");
  IntRect rect =
      AbsoluteSelectionBoundsOf(VisiblePosition::Create(PositionWithAffinity(
          Position(node, PositionAnchorType::kAfterChildren))));
  EXPECT_FALSE(rect.IsEmpty());
}

}  // namespace blink
