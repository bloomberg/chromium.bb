// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SnapCoordinator.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include <gtest/gtest.h>
#include <memory>

namespace blink {

using HTMLNames::styleAttr;

typedef bool TestParamRootLayerScrolling;
class SnapCoordinatorTest
    : public testing::TestWithParam<TestParamRootLayerScrolling>,
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
        "        scroll-snap-type: mandatory;"
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

  SnapCoordinator& Coordinator() { return *GetDocument().GetSnapCoordinator(); }

  Vector<double> SnapOffsets(const ContainerNode& node,
                             ScrollbarOrientation orientation) {
    return Coordinator().SnapOffsets(node, orientation);
  }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

INSTANTIATE_TEST_CASE_P(All, SnapCoordinatorTest, ::testing::Bool());

TEST_P(SnapCoordinatorTest, ValidRepeat) {
  SnapContainer().setAttribute(styleAttr,
                               "scroll-snap-points-x: repeat(20%); "
                               "scroll-snap-points-y: repeat(400px);");
  GetDocument().UpdateStyleAndLayout();
  {
    const int expected_step_size = SnapContainer().clientWidth() * 0.2;
    Vector<double> actual = SnapOffsets(SnapContainer(), kHorizontalScrollbar);
    EXPECT_EQ(5U, actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
      EXPECT_EQ((i + 1) * expected_step_size, actual[i]);
  }
  {
    Vector<double> actual = SnapOffsets(SnapContainer(), kVerticalScrollbar);
    EXPECT_EQ(2U, actual.size());
    EXPECT_EQ(400, actual[0]);
    EXPECT_EQ(800, actual[1]);
  }
}

TEST_P(SnapCoordinatorTest, EmptyRepeat) {
  SnapContainer().setAttribute(
      styleAttr, "scroll-snap-points-x: none; scroll-snap-points-y: none;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kHorizontalScrollbar).size());
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kVerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, ZeroAndNegativeRepeat) {
  // These be rejected as an invalid repeat values thus no snap offset is
  // created.
  SnapContainer().setAttribute(
      styleAttr,
      "scroll-snap-points-x: repeat(-1px); scroll-snap-points-y: repeat(0px);");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kHorizontalScrollbar).size());
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kVerticalScrollbar).size());

  // Calc values are not be rejected outright but instead clamped to 1px min
  // repeat value.
  SnapContainer().setAttribute(styleAttr,
                               "scroll-snap-points-x: repeat(calc(10px - "
                               "100%)); scroll-snap-points-y: "
                               "repeat(calc(0px));");
  GetDocument().UpdateStyleAndLayout();

  // A repeat value of 1px should give us |(scroll size - client size) / 1| snap
  // offsets.
  unsigned expected_horizontal_snap_offsets =
      SnapContainer().scrollWidth() - SnapContainer().clientWidth();
  EXPECT_EQ(expected_horizontal_snap_offsets,
            SnapOffsets(SnapContainer(), kHorizontalScrollbar).size());
  unsigned expected_vertical_snap_offsets =
      SnapContainer().scrollHeight() - SnapContainer().clientHeight();
  EXPECT_EQ(expected_vertical_snap_offsets,
            SnapOffsets(SnapContainer(), kVerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, SimpleSnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-coordinate: 10px 11px;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(10, SnapOffsets(SnapContainer(), kHorizontalScrollbar)[0]);
  EXPECT_EQ(11, SnapOffsets(SnapContainer(), kVerticalScrollbar)[0]);

  // Multiple coordinate and translates
  snap_element.setAttribute(styleAttr,
                            "scroll-snap-coordinate: 20px 21px, 40px 41px; "
                            "transform: translate(10px, 10px);");
  GetDocument().UpdateStyleAndLayout();

  Vector<double> result = SnapOffsets(SnapContainer(), kHorizontalScrollbar);
  EXPECT_EQ(30, result[0]);
  EXPECT_EQ(50, result[1]);
  result = SnapOffsets(SnapContainer(), kVerticalScrollbar);
  EXPECT_EQ(31, result[0]);
  EXPECT_EQ(51, result[1]);
}

TEST_P(SnapCoordinatorTest, NestedSnapElement) {
  Element& snap_element = *GetDocument().getElementById("nested-snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-coordinate: 20px 25px;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(20, SnapOffsets(SnapContainer(), kHorizontalScrollbar)[0]);
  EXPECT_EQ(25, SnapOffsets(SnapContainer(), kVerticalScrollbar)[0]);
}

TEST_P(SnapCoordinatorTest, NestedSnapElementCaptured) {
  Element& snap_element = *GetDocument().getElementById("nested-snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-coordinate: 20px 25px;");

  Element* intermediate = GetDocument().getElementById("intermediate");
  intermediate->setAttribute(styleAttr, "overflow: scroll;");

  GetDocument().UpdateStyleAndLayout();

  // Intermediate scroller captures nested snap elements first so ancestor
  // does not get them.
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kHorizontalScrollbar).size());
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kVerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, PositionFixedSnapElement) {
  Element& snap_element =
      *GetDocument().getElementById("snap-element-fixed-position");
  snap_element.setAttribute(styleAttr, "scroll-snap-coordinate: 1px 1px;");
  GetDocument().UpdateStyleAndLayout();

  // Position fixed elements are contained in document and not its immediate
  // ancestor scroller. They cannot be a valid snap destination so they should
  // not contribute snap points to their immediate snap container or document
  // See: https://lists.w3.org/Archives/Public/www-style/2015Jun/0376.html
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kHorizontalScrollbar).size());
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kVerticalScrollbar).size());

  Element* body = GetDocument().ViewportDefiningElement();
  EXPECT_EQ(0U, SnapOffsets(*body, kHorizontalScrollbar).size());
  EXPECT_EQ(0U, SnapOffsets(*body, kVerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, RepeatAndSnapElementTogether) {
  GetDocument()
      .getElementById("snap-element")
      ->setAttribute(styleAttr, "scroll-snap-coordinate: 5px 10px;");
  GetDocument()
      .getElementById("nested-snap-element")
      ->setAttribute(styleAttr, "scroll-snap-coordinate: 250px 450px;");

  SnapContainer().setAttribute(styleAttr,
                               "scroll-snap-points-x: repeat(200px); "
                               "scroll-snap-points-y: repeat(400px);");
  GetDocument().UpdateStyleAndLayout();

  {
    Vector<double> result = SnapOffsets(SnapContainer(), kHorizontalScrollbar);
    EXPECT_EQ(7U, result.size());
    EXPECT_EQ(5, result[0]);
    EXPECT_EQ(200, result[1]);
    EXPECT_EQ(250, result[2]);
  }
  {
    Vector<double> result = SnapOffsets(SnapContainer(), kVerticalScrollbar);
    EXPECT_EQ(4U, result.size());
    EXPECT_EQ(10, result[0]);
    EXPECT_EQ(400, result[1]);
    EXPECT_EQ(450, result[2]);
  }
}

TEST_P(SnapCoordinatorTest, SnapPointsAreScrollOffsetIndependent) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-coordinate: 10px 11px;");
  SnapContainer().scrollBy(100, 100);
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(SnapContainer().scrollLeft(), 100);
  EXPECT_EQ(SnapContainer().scrollTop(), 100);
  EXPECT_EQ(10, SnapOffsets(SnapContainer(), kHorizontalScrollbar)[0]);
  EXPECT_EQ(11, SnapOffsets(SnapContainer(), kVerticalScrollbar)[0]);
}

TEST_P(SnapCoordinatorTest, UpdateStyleForSnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(styleAttr, "scroll-snap-coordinate: 10px 11px;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(10, SnapOffsets(SnapContainer(), kHorizontalScrollbar)[0]);
  EXPECT_EQ(11, SnapOffsets(SnapContainer(), kVerticalScrollbar)[0]);

  snap_element.remove();
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kHorizontalScrollbar).size());
  EXPECT_EQ(0U, SnapOffsets(SnapContainer(), kVerticalScrollbar).size());

  // Add a new snap element
  Element& container = *GetDocument().getElementById("snap-container");
  container.setInnerHTML(
      "<div style='scroll-snap-coordinate: 20px 22px;'><div "
      "style='width:2000px; height:2000px;'></div></div>");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(20, SnapOffsets(SnapContainer(), kHorizontalScrollbar)[0]);
  EXPECT_EQ(22, SnapOffsets(SnapContainer(), kVerticalScrollbar)[0]);
}

TEST_P(SnapCoordinatorTest, LayoutViewCapturesWhenBodyElementViewportDefining) {
  SetHTML(
      "<style>"
      "body {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 1000px;"
      "    width: 1000px;"
      "    margin: 5px;"
      "}"
      "</style>"
      "<body>"
      "    <div id='snap-element' style='scroll-snap-coordinate: 5px "
      "6px;'></div>"
      "    <div>"
      "       <div id='nested-snap-element' style='scroll-snap-coordinate: "
      "10px 11px;'></div>"
      "    </div>"
      "    <div style='width:2000px; height:2000px;'></div>"
      "</body>");

  GetDocument().UpdateStyleAndLayout();

  // Sanity check that body is the viewport defining element
  EXPECT_EQ(GetDocument().body(), GetDocument().ViewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // body element will be captured by layout view as the snap container.
  Vector<double> result = SnapOffsets(GetDocument(), kHorizontalScrollbar);
  EXPECT_EQ(10, result[0]);
  EXPECT_EQ(15, result[1]);
  result = SnapOffsets(GetDocument(), kVerticalScrollbar);
  EXPECT_EQ(11, result[0]);
  EXPECT_EQ(16, result[1]);
}

TEST_P(SnapCoordinatorTest,
       LayoutViewCapturesWhenDocumentElementViewportDefining) {
  SetHTML(
      "<style>"
      ":root {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 500px;"
      "    width: 500px;"
      "}"
      "body {"
      "    margin: 5px;"
      "}"
      "</style>"
      "<html>"
      "   <body>"
      "       <div id='snap-element' style='scroll-snap-coordinate: 5px "
      "6px;'></div>"
      "       <div>"
      "         <div id='nested-snap-element' style='scroll-snap-coordinate: "
      "10px 11px;'></div>"
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
  Vector<double> result = SnapOffsets(GetDocument(), kHorizontalScrollbar);
  EXPECT_EQ(10, result[0]);
  EXPECT_EQ(15, result[1]);
  result = SnapOffsets(GetDocument(), kVerticalScrollbar);
  EXPECT_EQ(11, result[0]);
  EXPECT_EQ(16, result[1]);
}

TEST_P(SnapCoordinatorTest,
       BodyCapturesWhenBodyOverflowAndDocumentElementViewportDefining) {
  SetHTML(
      "<style>"
      ":root {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 500px;"
      "    width: 500px;"
      "}"
      "body {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 1000px;"
      "    width: 1000px;"
      "    margin: 5px;"
      "}"
      "</style>"
      "<html>"
      "   <body style='overflow: scroll; scroll-snap-type: mandatory; "
      "height:1000px; width:1000px;'>"
      "       <div id='snap-element' style='scroll-snap-coordinate: 5px "
      "6px;'></div>"
      "       <div>"
      "         <div id='nested-snap-element' style='scroll-snap-coordinate: "
      "10px 11px;'></div>"
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
  Vector<double> result = SnapOffsets(body, kHorizontalScrollbar);
  EXPECT_EQ(5, result[0]);
  EXPECT_EQ(10, result[1]);
  result = SnapOffsets(body, kVerticalScrollbar);
  EXPECT_EQ(6, result[0]);
  EXPECT_EQ(11, result[1]);
}

}  // namespace
