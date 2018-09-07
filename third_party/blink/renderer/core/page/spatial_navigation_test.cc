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

  LayoutRect TopOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetHeight(LayoutUnit(0));
    return visual_viewport;
  }

  LayoutRect BottomOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetY(visual_viewport.MaxY());
    visual_viewport.SetHeight(LayoutUnit(0));
    return visual_viewport;
  }

  LayoutRect LeftSideOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetWidth(LayoutUnit(0));
    return visual_viewport;
  }

  LayoutRect RightSideOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetX(visual_viewport.MaxX());
    visual_viewport.SetWidth(LayoutUnit(0));
    return visual_viewport;
  }
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
      "<a id='child'>link</a>");

  ChildDocument().View()->UpdateAllLifecyclePhases();
  Element* child_element = ChildDocument().getElementById("child");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);

  EXPECT_FALSE(IsRectOffscreen(enclosing_container));
  EXPECT_FALSE(IsRectOffscreen(child_element));

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

  EXPECT_TRUE(IsRectOffscreen(content_element));
  EXPECT_FALSE(IsRectOffscreen(container_element));

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

TEST_F(SpatialNavigationTest, RootViewportRespectsVisibleSize) {
  EXPECT_EQ(RootViewport(&GetFrame()), LayoutRect(0, 0, 800, 600));

  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetSize({123, 123});
  EXPECT_EQ(RootViewport(&GetFrame()), LayoutRect(0, 0, 123, 123));
}

TEST_F(SpatialNavigationTest, StartAtVisibleFocusedElement) {
  SetBodyInnerHTML("<button id='b'>hello</button>");
  Element* b = GetDocument().getElementById("b");

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeDown),
            NodeRectInRootFrame(b, true));
}

TEST_F(SpatialNavigationTest, StartAtTopWhenGoingDownwardsWithoutFocus) {
  EXPECT_EQ(LayoutRect(0, 0, 111, 0),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeDown));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeDown),
            TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtBottomWhenGoingUpwardsWithoutFocus) {
  EXPECT_EQ(LayoutRect(0, 222, 111, 0),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeUp));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeUp),
            BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtLeftSideWhenGoingEastWithoutFocus) {
  EXPECT_EQ(LayoutRect(0, 0, 0, 222),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeRight));

  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeRight),
      LeftSideOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtRightSideWhenGoingWestWithoutFocus) {
  EXPECT_EQ(LayoutRect(111, 0, 0, 222),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeLeft));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeLeft),
            RightSideOfVisualViewport());
}

TEST_F(SpatialNavigationTest,
       StartAtBottomWhenGoingUpwardsAndFocusIsOffscreen) {
  SetBodyInnerHTML(
      "<button id='b' style='margin-top: 120%;'>B</button>");  // Outside visual
                                                               // viewport.
  Element* b = GetDocument().getElementById("b");
  EXPECT_TRUE(IsRectOffscreen(b));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtContainersEdge) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "    height: 100px;"
      "    width: 100px;"
      "    overflow: scroll;"
      "  }"
      "  button {"
      "    margin-top: 300px;"  // Outside div's scrollport.
      "  }"
      "</style>"
      "<div id='container'>"
      "  <button id='b'>B</button>"
      "</div>");

  Element* b = GetDocument().getElementById("b");
  const Element* container = GetDocument().getElementById("container");
  const LayoutRect container_box = NodeRectInRootFrame(container, true);

  EXPECT_TRUE(IsRectOffscreen(b));

  // Go down.
  LayoutRect container_top_edge = container_box;
  container_top_edge.SetHeight(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeDown),
            container_top_edge);

  // Go up.
  LayoutRect container_bottom_edge = container_box;
  container_bottom_edge.SetY(container_bottom_edge.MaxX());
  container_bottom_edge.SetHeight(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            container_bottom_edge);

  // Go right.
  LayoutRect container_leftmost_edge = container_box;
  container_leftmost_edge.SetWidth(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeRight),
            container_leftmost_edge);

  // Go left.
  LayoutRect container_rightmost_edge = container_box;
  container_rightmost_edge.SetX(container_bottom_edge.MaxX());
  container_rightmost_edge.SetWidth(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeLeft),
            container_rightmost_edge);
}

TEST_F(SpatialNavigationTest,
       StartFromDocEdgeWhenFocusIsClippedInOffscreenScroller) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "    margin-top: 120%;"  // Outside visual viewport.
      "    height: 100px;"
      "    width: 100px;"
      "    overflow: scroll;"
      "  }"
      "  button {"
      "    margin-top: 300px;"  // Outside div's scrollport.
      "  }"
      "</style>"
      "<div id='scroller'>"
      "  <button id='b'>B</button>"
      "</div>");

  Element* scroller = GetDocument().getElementById("scroller");
  Element* b = GetDocument().getElementById("b");

  EXPECT_TRUE(IsRectOffscreen(scroller));
  EXPECT_TRUE(IsRectOffscreen(b));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            BottomOfVisualViewport());
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeDown),
            TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest,
       StartFromDocEdgeWhenFocusIsClippedInNestedOffscreenScroller) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "   margin-top: 1200px;"
      "   height: 100px;"
      "   width: 100px;"
      "   overflow: scroll;"
      "}"
      "a {"
      "  display: block;"
      "  margin-top: 300px;"
      "}"
      "</style>"
      "<div id='scroller1'>"
      "  <div id='scroller2'>"
      "    <a id='link'>link</a>"
      "  </div>"
      "</div>");

  Element* scroller1 = GetDocument().getElementById("scroller1");
  Element* scroller2 = GetDocument().getElementById("scroller2");
  Element* link = GetDocument().getElementById("link");

  EXPECT_TRUE(IsScrollableAreaOrDocument(scroller1));
  EXPECT_TRUE(IsScrollableAreaOrDocument(scroller2));
  EXPECT_TRUE(IsRectOffscreen(scroller1));
  EXPECT_TRUE(IsRectOffscreen(scroller1));
  EXPECT_TRUE(IsRectOffscreen(link));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), link, kWebFocusTypeUp),
            BottomOfVisualViewport());
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), link, kWebFocusTypeDown),
            TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest, PartiallyVisible) {
  // <button>'s bottom is clipped.
  SetBodyInnerHTML("<button id='b' style='height: 900px;'>B</button>");
  Element* b = GetDocument().getElementById("b");

  EXPECT_FALSE(IsRectOffscreen(b));  // <button> is not completely offscreen.

  LayoutRect button_in_root_frame = NodeRectInRootFrame(b, true);

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            Intersection(button_in_root_frame, RootViewport(&GetFrame())));

  // Do some scrolling.
  ScrollableArea* root_scroller = GetDocument().View()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(0, 600), kProgrammaticScroll);
  LayoutRect button_after_scroll = NodeRectInRootFrame(b, true);
  ASSERT_NE(button_in_root_frame,
            button_after_scroll);  // As we scrolled, the
                                   // <button>'s position in
                                   // the root frame changed.

  // <button>'s top is clipped.
  EXPECT_FALSE(IsRectOffscreen(b));  // <button> is not completely offscreen.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            Intersection(button_after_scroll, RootViewport(&GetFrame())));
}

TEST_F(SpatialNavigationTest, PartiallyVisibleIFrame) {
  // <a> is off screen. The <iframe> is visible, but partially off screen.
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  iframe {"
      "    width: 200%;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #child {"
      "    margin-left: 120%;"
      "  }"
      "</style>"
      "<a id='child'>link</a>");

  ChildDocument().View()->UpdateAllLifecyclePhases();
  Element* child_element = ChildDocument().getElementById("child");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);
  EXPECT_EQ(enclosing_container, ChildDocument());

  EXPECT_TRUE(IsRectOffscreen(child_element));         // Completely offscreen.
  EXPECT_FALSE(IsRectOffscreen(enclosing_container));  // Partially visible.

  LayoutRect iframe = NodeRectInRootFrame(enclosing_container, true);

  // When searching downwards we start at activeElement's
  // container's (here: the iframe's) topmost visible edge.
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), child_element, kWebFocusTypeDown),
      OppositeEdge(kWebFocusTypeDown,
                   Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching upwards we start at activeElement's
  // container's (here: the iframe's) bottommost visible edge.
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), child_element, kWebFocusTypeUp),
      OppositeEdge(kWebFocusTypeUp,
                   Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching eastwards, "to the right", we start at activeElement's
  // container's (here: the iframe's) leftmost visible edge.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), child_element,
                         kWebFocusTypeRight),
            OppositeEdge(kWebFocusTypeRight,
                         Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching westwards, "to the left", we start at activeElement's
  // container's (here: the iframe's) rightmost visible edge.
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), child_element, kWebFocusTypeLeft),
      OppositeEdge(kWebFocusTypeLeft,
                   Intersection(iframe, RootViewport(&GetFrame()))));
}

TEST_F(SpatialNavigationTest, BottomOfPinchedViewport) {
  LayoutRect origin =
      SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_EQ(origin.Width(), GetFrame().View()->Width());
  EXPECT_EQ(origin.X(), 0);
  EXPECT_EQ(origin.Y(), GetFrame().View()->Height());
  EXPECT_EQ(origin, BottomOfVisualViewport());

  // Now, test SearchOrigin with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_LT(origin.Y(), GetFrame().View()->Height());
  EXPECT_EQ(origin, BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, TopOfPinchedViewport) {
  LayoutRect origin =
      SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeDown);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_EQ(origin.Width(), GetFrame().View()->Width());
  EXPECT_EQ(origin.X(), 0);
  EXPECT_EQ(origin.Y(), 0);
  EXPECT_EQ(origin, TopOfVisualViewport());

  // Now, test SearchOrigin with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeDown);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_GT(origin.Y(), 0);
  EXPECT_EQ(origin, TopOfVisualViewport());
}
}  // namespace blink
