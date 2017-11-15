// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/SpatialNavigation.h"

#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SpatialNavigationTest : public RenderingTest {
 public:
  SpatialNavigationTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}
};

TEST_F(SpatialNavigationTest, FindContainerWhenEnclosingContainerIsDocument) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div id='child'></div>");

  Element* child_element = GetDocument().getElementById("child");

  Node* enclosing_container =
      ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
          WebFocusType::kWebFocusTypeDown, child_element);

  EXPECT_EQ(enclosing_container, GetDocument());
}

TEST_F(SpatialNavigationTest, FindContainerWhenEnclosingContainerIsIframe) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  iframe {"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #child {"
      "    height: 1000px;"
      "  }"
      "</style>"
      "<div id='child'></div>");
  ChildDocument().View()->UpdateAllLifecyclePhases();

  Element* child_element = ChildDocument().getElementById("child");

  Node* enclosing_container =
      ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
          WebFocusType::kWebFocusTypeDown, child_element);

  EXPECT_EQ(enclosing_container, ChildDocument());
}

TEST_F(SpatialNavigationTest,
       FindContainerWhenEnclosingContainerIsScrollableOverflowBox) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #child {"
      "    height: 1000px;"
      "  }"
      "  #container {"
      "    width: 100%;"
      "    height: 100%;"
      "    overflow-y: scroll;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='child'></div>"
      "</div>");

  {
    // <div id='container'> should be selected as container because
    // it is scrollable in direction.
    Element* child_element = GetDocument().getElementById("child");
    Element* container_element = GetDocument().getElementById("container");

    Node* enclosing_container =
        ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
            WebFocusType::kWebFocusTypeDown, child_element);
    EXPECT_EQ(enclosing_container, container_element);
  }

  {
    // In following cases(up, left, right), <div id='container'> cannot be
    // container because it is not scrollable in direction.
    Element* child_element = GetDocument().getElementById("child");

    Node* enclosing_container =
        ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
            WebFocusType::kWebFocusTypeUp, child_element);
    EXPECT_EQ(enclosing_container, GetDocument());

    enclosing_container = ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
        WebFocusType::kWebFocusTypeLeft, child_element);
    EXPECT_EQ(enclosing_container, GetDocument());

    enclosing_container = ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
        WebFocusType::kWebFocusTypeRight, child_element);
    EXPECT_EQ(enclosing_container, GetDocument());
  }
}

}  // namespace blink
