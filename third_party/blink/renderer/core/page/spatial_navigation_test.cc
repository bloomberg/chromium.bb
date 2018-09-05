// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class SpatialNavigationTest : public RenderingTest {
 public:
  SpatialNavigationTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}
};

TEST_F(SpatialNavigationTest, FindContainerWhenEnclosingContainerIsDocument) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<a id='child'>link</a>");

  Element* child_element = GetDocument().getElementById("child");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);

  EXPECT_EQ(enclosing_container, GetDocument());
  EXPECT_TRUE(IsScrollableAreaOrDocument(enclosing_container));
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
      "<a id='child'>link</a>");

  ChildDocument().View()->UpdateAllLifecyclePhases();
  Element* child_element = ChildDocument().getElementById("child");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);

  EXPECT_EQ(enclosing_container, ChildDocument());
  EXPECT_TRUE(IsScrollableAreaOrDocument(enclosing_container));
}

TEST_F(SpatialNavigationTest,
       FindContainerWhenEnclosingContainerIsScrollableOverflowBox) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #content {"
      "    margin-top: 600px;"
      "  }"
      "  #container {"
      "    height: 100px;"
      "    overflow-y: scroll;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='content'>some text here</div>"
      "</div>");

  Element* content_element = GetDocument().getElementById("content");
  Element* container_element = GetDocument().getElementById("container");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(content_element);

  EXPECT_EQ(enclosing_container, container_element);
  EXPECT_TRUE(IsScrollableAreaOrDocument(enclosing_container));
}

TEST_F(SpatialNavigationTest, ZooomPutsElementOffScreen) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<button id='a'>hello</button><br>"
      "<button id='b' style='margin-top: 70%'>bello</button>");

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");
  EXPECT_FALSE(IsRectOffscreen(a));
  EXPECT_FALSE(IsRectOffscreen(b));

  // Now, test IsRectOffscreen with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  // #b is no longer visible.
  EXPECT_FALSE(IsRectOffscreen(a));
  EXPECT_TRUE(IsRectOffscreen(b));
}
}  // namespace blink
