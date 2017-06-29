// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/LayoutTreeBuilderTraversal.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutText.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutTreeBuilderTraversalTest : public RenderingTest {
 protected:
  void SetupSampleHTML(const char* main_html);
};

void LayoutTreeBuilderTraversalTest::SetupSampleHTML(const char* main_html) {
  SetBodyInnerHTML(String::FromUTF8(main_html));
}

TEST_F(LayoutTreeBuilderTraversalTest, emptySubTree) {
  const char* const kHtml = "<div id='top'></div>";
  SetupSampleHTML(kHtml);

  Element* top = GetDocument().QuerySelector("#top");
  Element* body = GetDocument().QuerySelector("body");
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::FirstChild(*top));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::NextSibling(*top));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::PreviousSibling(*top));
  EXPECT_EQ(body, LayoutTreeBuilderTraversal::Parent(*top));
}

TEST_F(LayoutTreeBuilderTraversalTest, pseudos) {
  const char* const kHtml =
      "<style>"
      "#top::before { content: \"foo\"; }"
      "#top::before { content: \"bar\"; }"
      "</style>"
      "<div id='top'></div>";
  SetupSampleHTML(kHtml);

  Element* top = GetDocument().QuerySelector("#top");
  Element* before = top->GetPseudoElement(kPseudoIdBefore);
  Element* after = top->GetPseudoElement(kPseudoIdAfter);
  EXPECT_EQ(before, LayoutTreeBuilderTraversal::Next(*top, nullptr));
  EXPECT_EQ(after, LayoutTreeBuilderTraversal::NextSibling(*before));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::PreviousSibling(*before));
}

TEST_F(LayoutTreeBuilderTraversalTest, emptyDisplayContents) {
  const char* const kHtml =
      "<div></div>"
      "<div style='display: contents'></div>"
      "<div id='last'></div>";
  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector("div");
  Element* last = GetDocument().QuerySelector("#last");

  EXPECT_TRUE(last->GetLayoutObject());
  EXPECT_EQ(last->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first));
}

TEST_F(LayoutTreeBuilderTraversalTest, displayContentsChildren) {
  const char* const kHtml =
      "<div></div>"
      "<div id='contents' style='display: contents'><div "
      "id='inner'></div></div>"
      "<div id='last'></div>";
  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector("div");
  Element* inner = GetDocument().QuerySelector("#inner");
  Element* contents = GetDocument().QuerySelector("#contents");
  Element* last = GetDocument().QuerySelector("#last");

  EXPECT_TRUE(inner->GetLayoutObject());
  EXPECT_TRUE(last->GetLayoutObject());
  EXPECT_TRUE(first->GetLayoutObject());
  EXPECT_FALSE(contents->GetLayoutObject());

  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first));
  EXPECT_EQ(first->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*inner));

  EXPECT_EQ(last->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*inner));
  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*last));
}

TEST_F(LayoutTreeBuilderTraversalTest, displayContentsChildrenNested) {
  const char* const kHtml =
      "<div></div>"
      "<div style='display: contents'>"
      "<div style='display: contents'>"
      "<div id='inner'></div>"
      "<div id='inner-sibling'></div>"
      "</div>"
      "</div>"
      "<div id='last'></div>";
  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector("div");
  Element* inner = GetDocument().QuerySelector("#inner");
  Element* sibling = GetDocument().QuerySelector("#inner-sibling");
  Element* last = GetDocument().QuerySelector("#last");

  EXPECT_TRUE(first->GetLayoutObject());
  EXPECT_TRUE(inner->GetLayoutObject());
  EXPECT_TRUE(sibling->GetLayoutObject());
  EXPECT_TRUE(last->GetLayoutObject());

  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first));
  EXPECT_EQ(first->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*inner));

  EXPECT_EQ(sibling->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*inner));
  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*sibling));

  EXPECT_EQ(last->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*sibling));
  EXPECT_EQ(sibling->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*last));
}

TEST_F(LayoutTreeBuilderTraversalTest, limits) {
  const char* const kHtml =
      "<div></div>"
      "<div style='display: contents'></div>"
      "<div style='display: contents'>"
      "<div style='display: contents'>"
      "</div>"
      "</div>"
      "<div id='shouldNotBeFound'></div>";

  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector("div");

  EXPECT_TRUE(first->GetLayoutObject());
  LayoutObject* next_sibling =
      LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first, 2);
  EXPECT_FALSE(next_sibling);  // Should not overrecurse
}

}  // namespace blink
