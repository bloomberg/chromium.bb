// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/LayoutTreeBuilderTraversal.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/PseudoElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutText.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutTreeBuilderTraversalTest : public RenderingTest {
 protected:
  void setupSampleHTML(const char* mainHTML);
};

void LayoutTreeBuilderTraversalTest::setupSampleHTML(const char* mainHTML) {
  setBodyInnerHTML(String::fromUTF8(mainHTML));
}

TEST_F(LayoutTreeBuilderTraversalTest, emptySubTree) {
  const char* const html = "<div id='top'></div>";
  setupSampleHTML(html);

  Element* top = document().querySelector("#top");
  Element* body = document().querySelector("body");
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::firstChild(*top));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::nextSibling(*top));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::previousSibling(*top));
  EXPECT_EQ(body, LayoutTreeBuilderTraversal::parent(*top));
}

TEST_F(LayoutTreeBuilderTraversalTest, pseudos) {
  const char* const html =
      "<style>"
      "#top::before { content: \"foo\"; }"
      "#top::before { content: \"bar\"; }"
      "</style>"
      "<div id='top'></div>";
  setupSampleHTML(html);

  Element* top = document().querySelector("#top");
  Element* before = top->pseudoElement(PseudoIdBefore);
  Element* after = top->pseudoElement(PseudoIdAfter);
  EXPECT_EQ(before, LayoutTreeBuilderTraversal::next(*top, nullptr));
  EXPECT_EQ(after, LayoutTreeBuilderTraversal::nextSibling(*before));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::previousSibling(*before));
}

TEST_F(LayoutTreeBuilderTraversalTest, emptyDisplayContents) {
  const char* const html =
      "<div></div>"
      "<div style='display: contents'></div>"
      "<div id='last'></div>";
  setupSampleHTML(html);

  Element* first = document().querySelector("div");
  Element* last = document().querySelector("#last");

  EXPECT_TRUE(last->layoutObject());
  EXPECT_EQ(last->layoutObject(),
            LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*first));
}

TEST_F(LayoutTreeBuilderTraversalTest, displayContentsChildren) {
  const char* const html =
      "<div></div>"
      "<div id='contents' style='display: contents'><div "
      "id='inner'></div></div>"
      "<div id='last'></div>";
  setupSampleHTML(html);

  Element* first = document().querySelector("div");
  Element* inner = document().querySelector("#inner");
  Element* contents = document().querySelector("#contents");
  Element* last = document().querySelector("#last");

  EXPECT_TRUE(inner->layoutObject());
  EXPECT_TRUE(last->layoutObject());
  EXPECT_TRUE(first->layoutObject());
  EXPECT_FALSE(contents->layoutObject());

  EXPECT_EQ(inner->layoutObject(),
            LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*first));
  EXPECT_EQ(first->layoutObject(),
            LayoutTreeBuilderTraversal::previousSiblingLayoutObject(*inner));

  EXPECT_EQ(last->layoutObject(),
            LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*inner));
  EXPECT_EQ(inner->layoutObject(),
            LayoutTreeBuilderTraversal::previousSiblingLayoutObject(*last));
}

TEST_F(LayoutTreeBuilderTraversalTest, displayContentsChildrenNested) {
  const char* const html =
      "<div></div>"
      "<div style='display: contents'>"
      "<div style='display: contents'>"
      "<div id='inner'></div>"
      "<div id='inner-sibling'></div>"
      "</div>"
      "</div>"
      "<div id='last'></div>";
  setupSampleHTML(html);

  Element* first = document().querySelector("div");
  Element* inner = document().querySelector("#inner");
  Element* sibling = document().querySelector("#inner-sibling");
  Element* last = document().querySelector("#last");

  EXPECT_TRUE(first->layoutObject());
  EXPECT_TRUE(inner->layoutObject());
  EXPECT_TRUE(sibling->layoutObject());
  EXPECT_TRUE(last->layoutObject());

  EXPECT_EQ(inner->layoutObject(),
            LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*first));
  EXPECT_EQ(first->layoutObject(),
            LayoutTreeBuilderTraversal::previousSiblingLayoutObject(*inner));

  EXPECT_EQ(sibling->layoutObject(),
            LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*inner));
  EXPECT_EQ(inner->layoutObject(),
            LayoutTreeBuilderTraversal::previousSiblingLayoutObject(*sibling));

  EXPECT_EQ(last->layoutObject(),
            LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*sibling));
  EXPECT_EQ(sibling->layoutObject(),
            LayoutTreeBuilderTraversal::previousSiblingLayoutObject(*last));
}

TEST_F(LayoutTreeBuilderTraversalTest, limits) {
  const char* const html =
      "<div></div>"
      "<div style='display: contents'></div>"
      "<div style='display: contents'>"
      "<div style='display: contents'>"
      "</div>"
      "</div>"
      "<div id='shouldNotBeFound'></div>";

  setupSampleHTML(html);

  Element* first = document().querySelector("div");

  EXPECT_TRUE(first->layoutObject());
  LayoutObject* nextSibling =
      LayoutTreeBuilderTraversal::nextSiblingLayoutObject(*first, 2);
  EXPECT_FALSE(nextSibling);  // Should not overrecurse
}

}  // namespace blink
