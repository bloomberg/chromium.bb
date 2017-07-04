// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SnapCoordinator.h"

#include <gtest/gtest.h>
#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutBox.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

using HTMLNames::styleAttr;

typedef bool TestParamRootLayerScrolling;
class SnapCoordinatorTest
    : public ::testing::TestWithParam<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest {
 protected:
  SnapCoordinatorTest() : ScopedRootLayerScrollingForTest(GetParam()) {}

  void SetUp() override {
    page_holder_ = DummyPageHolder::Create();

    SetHTML(
        "<style>"
        "    #snap-container {"
        "        height: 1000px;"
        "        width: 1000px;"
        "        overflow: scroll;"
        "        scroll-snap-type: both mandatory;"
        "    }"
        "    #snap-element-fixed-position {"
        "         position: fixed;"
        "    }"
        "</style>"
        "<body>"
        "  <div id='snap-container'>"
        "    <div id='snap-element'></div>"
        "    <div id='intermediate'>"
        "       <div id='nested-snap-element'></div>"
        "    </div>"
        "    <div id='snap-element-fixed-position'></div>"
        "    <div style='width:2000px; height:2000px;'></div>"
        "  </div>"
        "</body>");
    GetDocument().UpdateStyleAndLayout();
  }

  void TearDown() override { page_holder_ = nullptr; }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  void SetHTML(const char* html_content) {
    GetDocument().documentElement()->setInnerHTML(html_content);
  }

  Element& SnapContainer() {
    return *GetDocument().getElementById("snap-container");
  }

  unsigned SizeOfSnapAreas(const ContainerNode& node) {
    if (node.GetLayoutBox()->SnapAreas())
      return node.GetLayoutBox()->SnapAreas()->size();
    return 0U;
  }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

INSTANTIATE_TEST_CASE_P(All, SnapCoordinatorTest, ::testing::Bool());

TEST_P(SnapCoordinatorTest, SimpleSnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_P(SnapCoordinatorTest, NestedSnapElement) {
  Element& snap_element = *GetDocument().getElementById("nested-snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_P(SnapCoordinatorTest, NestedSnapElementCaptured) {
  Element& snap_element = *GetDocument().getElementById("nested-snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-align: start;");

  Element* intermediate = GetDocument().getElementById("intermediate");
  intermediate->setAttribute(styleAttr, "overflow: scroll;");

  GetDocument().UpdateStyleAndLayout();

  // Intermediate scroller captures nested snap elements first so ancestor
  // does not get them.
  EXPECT_EQ(0U, SizeOfSnapAreas(SnapContainer()));
  EXPECT_EQ(1U, SizeOfSnapAreas(*intermediate));
}

TEST_P(SnapCoordinatorTest, PositionFixedSnapElement) {
  Element& snap_element =
      *GetDocument().getElementById("snap-element-fixed-position");
  snap_element.setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();

  // Position fixed elements are contained in document and not its immediate
  // ancestor scroller. They cannot be a valid snap destination so they should
  // not contribute snap points to their immediate snap container or document
  // See: https://lists.w3.org/Archives/Public/www-style/2015Jun/0376.html
  EXPECT_EQ(0U, SizeOfSnapAreas(SnapContainer()));

  Element* body = GetDocument().ViewportDefiningElement();
  EXPECT_EQ(0U, SizeOfSnapAreas(*body));
}

TEST_P(SnapCoordinatorTest, UpdateStyleForSnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));

  snap_element.remove();
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(0U, SizeOfSnapAreas(SnapContainer()));

  // Add a new snap element
  Element& container = *GetDocument().getElementById("snap-container");
  container.setInnerHTML(
      "<div style='scroll-snap-align: start;'>"
      "    <div style='width:2000px; height:2000px;'></div>"
      "</div>");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_P(SnapCoordinatorTest, LayoutViewCapturesWhenBodyElementViewportDefining) {
  SetHTML(
      "<style>"
      "body {"
      "    overflow: scroll;"
      "    scroll-snap-type: both mandatory;"
      "    height: 1000px;"
      "    width: 1000px;"
      "    margin: 5px;"
      "}"
      "</style>"
      "<body>"
      "    <div id='snap-element' style='scroll-snap-align: start;></div>"
      "    <div id='intermediate'>"
      "        <div id='nested-snap-element'"
      "            style='scroll-snap-align: start;'></div>"
      "    </div>"
      "    <div style='width:2000px; height:2000px;'></div>"
      "</body>");

  GetDocument().UpdateStyleAndLayout();

  // Sanity check that body is the viewport defining element
  EXPECT_EQ(GetDocument().body(), GetDocument().ViewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // body element will be captured by layout view as the snap container.
  EXPECT_EQ(2U, SizeOfSnapAreas(GetDocument()));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().body())));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().documentElement())));
}

TEST_P(SnapCoordinatorTest,
       LayoutViewCapturesWhenDocumentElementViewportDefining) {
  SetHTML(
      "<style>"
      ":root {"
      "    overflow: scroll;"
      "    scroll-snap-type: both mandatory;"
      "    height: 500px;"
      "    width: 500px;"
      "}"
      "body {"
      "    margin: 5px;"
      "}"
      "</style>"
      "<html>"
      "   <body>"
      "       <div id='snap-element' style='scroll-snap-align: start;></div>"
      "       <div id='intermediate'>"
      "         <div id='nested-snap-element'"
      "             style='scroll-snap-align: start;'></div>"
      "      </div>"
      "      <div style='width:2000px; height:2000px;'></div>"
      "   </body>"
      "</html>");

  GetDocument().UpdateStyleAndLayout();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // the document element will be captured by layout view as the snap
  // container.
  EXPECT_EQ(2U, SizeOfSnapAreas(GetDocument()));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().body())));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().documentElement())));
}

TEST_P(SnapCoordinatorTest,
       BodyCapturesWhenBodyOverflowAndDocumentElementViewportDefining) {
  SetHTML(
      "<style>"
      ":root {"
      "    overflow: scroll;"
      "    scroll-snap-type: both mandatory;"
      "    height: 500px;"
      "    width: 500px;"
      "}"
      "body {"
      "    overflow: scroll;"
      "    scroll-snap-type: both mandatory;"
      "    height: 1000px;"
      "    width: 1000px;"
      "    margin: 5px;"
      "}"
      "</style>"
      "<html>"
      "   <body style='overflow: scroll; scroll-snap-type: both mandatory; "
      "height:1000px; width:1000px;'>"
      "       <div id='snap-element' style='scroll-snap-align: start;></div>"
      "       <div id='intermediate'>"
      "         <div id='nested-snap-element'"
      "             style='scroll-snap-align: start;'></div>"
      "      </div>"
      "      <div style='width:2000px; height:2000px;'></div>"
      "   </body>"
      "</html>");

  GetDocument().UpdateStyleAndLayout();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When body and document elements are both scrollable then body element
  // should capture snap points defined on it as opposed to layout view.
  Element& body = *GetDocument().body();
  EXPECT_EQ(2U, SizeOfSnapAreas(body));
}

}  // namespace
