// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/LocalCaretRect.h"

#include "core/editing/PositionWithAffinity.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class LocalCaretRectTest : public EditingTestBase {};

TEST_F(LocalCaretRectTest, DOMAndFlatTrees) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");

  const LocalCaretRect& caret_rect_from_dom_tree =
      LocalCaretRectOfPosition(Position(one->firstChild(), 0));

  const LocalCaretRect& caret_rect_from_flat_tree =
      LocalCaretRectOfPosition(PositionInFlatTree(one->firstChild(), 0));

  EXPECT_FALSE(caret_rect_from_dom_tree.IsEmpty());
  EXPECT_EQ(caret_rect_from_dom_tree.layout_object,
            caret_rect_from_flat_tree.layout_object);
  EXPECT_EQ(caret_rect_from_dom_tree.rect, caret_rect_from_flat_tree.rect);
}

TEST_F(LocalCaretRectTest, SimpleText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>XXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(
      LayoutRect(0, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(10, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 1), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(20, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 2), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(29, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 3), TextAffinity::kDownstream})
          .rect);
}

TEST_F(LocalCaretRectTest, MixedHeightText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>Xpp</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(
      LayoutRect(0, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(10, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 1), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(20, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 2), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(29, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 3), TextAffinity::kDownstream})
          .rect);
}

TEST_F(LocalCaretRectTest, RtlText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl id=bdo style='display: block; "
      "font: 10px/10px Ahem; width: 30px'>XXX</bdo>");
  const Node* foo = GetElementById("bdo")->firstChild();

  EXPECT_EQ(
      LayoutRect(29, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(20, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 1), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(10, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 2), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(0, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 3), TextAffinity::kDownstream})
          .rect);
}

TEST_F(LocalCaretRectTest, VerticalText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='writing-mode: vertical-rl; "
      "font: 10px/10px Ahem; width: 30px'>Xpp</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(
      LayoutRect(0, 0, 10, 1),
      LocalCaretRectOfPosition({Position(foo, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(0, 10, 10, 1),
      LocalCaretRectOfPosition({Position(foo, 1), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(0, 20, 10, 1),
      LocalCaretRectOfPosition({Position(foo, 2), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(0, 29, 10, 1),
      LocalCaretRectOfPosition({Position(foo, 3), TextAffinity::kDownstream})
          .rect);
}

TEST_F(LocalCaretRectTest, TwoLinesOfTextWithSoftWrap) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px; "
      "word-break: break-all'>XXXXXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  // First line
  EXPECT_EQ(
      LayoutRect(0, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(10, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 1), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(20, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 2), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(29, 0, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 3), TextAffinity::kUpstream})
          .rect);

  // Second line
  EXPECT_EQ(
      LayoutRect(0, 10, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 3), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(10, 10, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 4), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(20, 10, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 5), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(29, 10, 1, 10),
      LocalCaretRectOfPosition({Position(foo, 6), TextAffinity::kDownstream})
          .rect);
}

TEST_F(LocalCaretRectTest, Images) {
  // This test only records the current behavior. Future changes are allowed.

  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>"
      "<img id=img1 width=10px height=10px>"
      "<img id=img2 width=10px height=10px>"
      "</div>");

  const Element& img1 = *GetElementById("img1");

  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(
                {Position::BeforeNode(img1), TextAffinity::kDownstream})
                .rect);
  EXPECT_EQ(LayoutRect(9, 0, 1, 10),
            LocalCaretRectOfPosition(
                {Position::AfterNode(img1), TextAffinity::kDownstream})
                .rect);

  const Element& img2 = *GetElementById("img2");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(
                {Position::BeforeNode(img2), TextAffinity::kDownstream})
                .rect);
  EXPECT_EQ(LayoutRect(9, 0, 1, 10),
            LocalCaretRectOfPosition(
                {Position::AfterNode(img2), TextAffinity::kDownstream})
                .rect);
}

TEST_F(LocalCaretRectTest, TextAndImageMixedHeight) {
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

  EXPECT_EQ(
      LayoutRect(0, 0, 1, 10),
      LocalCaretRectOfPosition({Position(text1, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(10, 0, 1, 10),
      LocalCaretRectOfPosition({Position(text1, 1), TextAffinity::kDownstream})
          .rect);

  EXPECT_EQ(LayoutRect(0, -5, 1, 10),
            LocalCaretRectOfPosition(
                {Position::BeforeNode(img), TextAffinity::kDownstream})
                .rect);
  EXPECT_EQ(LayoutRect(9, -5, 1, 10),
            LocalCaretRectOfPosition(
                {Position::AfterNode(img), TextAffinity::kDownstream})
                .rect);

  EXPECT_EQ(
      LayoutRect(20, 5, 1, 10),
      LocalCaretRectOfPosition({Position(text2, 0), TextAffinity::kDownstream})
          .rect);
  EXPECT_EQ(
      LayoutRect(29, 0, 1, 10),
      LocalCaretRectOfPosition({Position(text2, 1), TextAffinity::kDownstream})
          .rect);
}

}  // namespace blink
