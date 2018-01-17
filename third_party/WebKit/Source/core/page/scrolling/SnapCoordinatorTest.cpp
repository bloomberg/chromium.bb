// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/SnapCoordinator.h"

#include <gtest/gtest.h>
#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintLayerScrollableArea.h"
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

    SetHTML(R"HTML(
      <style>
          #snap-container {
              height: 1000px;
              width: 1000px;
              overflow: scroll;
              scroll-snap-type: both mandatory;
          }
          #snap-element-fixed-position {
               position: fixed;
          }
      </style>
      <body>
        <div id='snap-container'>
          <div id='snap-element'></div>
          <div id='intermediate'>
             <div id='nested-snap-element'></div>
          </div>
          <div id='snap-element-fixed-position'></div>
          <div style='width:2000px; height:2000px;'></div>
        </div>
      </body>
    )HTML");
    GetDocument().UpdateStyleAndLayout();
  }

  void TearDown() override { page_holder_ = nullptr; }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  void SetHTML(const char* html_content) {
    GetDocument().documentElement()->SetInnerHTMLFromString(html_content);
  }

  Element& SnapContainer() {
    return *GetDocument().getElementById("snap-container");
  }

  unsigned SizeOfSnapAreas(const ContainerNode& node) {
    if (node.GetLayoutBox()->SnapAreas())
      return node.GetLayoutBox()->SnapAreas()->size();
    return 0U;
  }

  void SetUpSingleSnapArea() {
    SetHTML(R"HTML(
      <style>
      #scroller {
        width: 140px;
        height: 160px;
        padding: 0px;
        scroll-snap-type: both mandatory;
        scroll-padding: 10px;
        overflow: scroll;
      }
      #container {
        margin: 0px;
        padding: 0px;
        width: 500px;
        height: 500px;
      }
      #area {
        position: relative;
        top: 200px;
        left: 200px;
        width: 100px;
        height: 100px;
        scroll-margin: 8px;
      }
      </style>
      <div id='scroller'>
        <div id='container'>
          <div id="area"></div>
        </div>
      </div>
      )HTML");
    GetDocument().UpdateStyleAndLayout();
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
  container.SetInnerHTMLFromString(R"HTML(
    <div style='scroll-snap-align: start;'>
        <div style='width:2000px; height:2000px;'></div>
    </div>
  )HTML");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_P(SnapCoordinatorTest, LayoutViewCapturesWhenBodyElementViewportDefining) {
  SetHTML(R"HTML(
    <style>
    body {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 1000px;
        width: 1000px;
        margin: 5px;
    }
    </style>
    <body>
        <div id='snap-element' style='scroll-snap-align: start;></div>
        <div id='intermediate'>
            <div id='nested-snap-element'
                style='scroll-snap-align: start;'></div>
        </div>
        <div style='width:2000px; height:2000px;'></div>
    </body>
  )HTML");

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
  SetHTML(R"HTML(
    <style>
    :root {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 500px;
        width: 500px;
    }
    body {
        margin: 5px;
    }
    </style>
    <html>
       <body>
           <div id='snap-element' style='scroll-snap-align: start;></div>
           <div id='intermediate'>
             <div id='nested-snap-element'
                 style='scroll-snap-align: start;'></div>
          </div>
          <div style='width:2000px; height:2000px;'></div>
       </body>
    </html>
  )HTML");

  GetDocument().UpdateStyleAndLayout();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When document is viewport defining and overflows then any snap points on
  // the document element will be captured by layout view as the snap
  // container.
  EXPECT_EQ(2U, SizeOfSnapAreas(GetDocument()));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().body())));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().documentElement())));
}

TEST_P(SnapCoordinatorTest,
       BodyCapturesWhenBodyOverflowAndDocumentElementViewportDefining) {
  SetHTML(R"HTML(
    <style>
    :root {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 500px;
        width: 500px;
    }
    body {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 1000px;
        width: 1000px;
        margin: 5px;
    }
    </style>
    <html>
       <body style='overflow: scroll; scroll-snap-type: both mandatory;
    height:1000px; width:1000px;'>
           <div id='snap-element' style='scroll-snap-align: start;></div>
           <div id='intermediate'>
             <div id='nested-snap-element'
                 style='scroll-snap-align: start;'></div>
          </div>
          <div style='width:2000px; height:2000px;'></div>
       </body>
    </html>
  )HTML");

  GetDocument().UpdateStyleAndLayout();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When body and document elements are both scrollable then body element
  // should capture snap points defined on it as opposed to layout view.
  Element& body = *GetDocument().body();
  EXPECT_EQ(2U, SizeOfSnapAreas(body));
}

#define EXPECT_EQ_CONTAINER(expected, actual)                          \
  {                                                                    \
    EXPECT_EQ(expected.max_position().x(), actual.max_position().x()); \
    EXPECT_EQ(expected.max_position().y(), actual.max_position().y()); \
    EXPECT_EQ(expected.scroll_snap_type(), actual.scroll_snap_type()); \
    EXPECT_EQ(expected.size(), actual.size());                         \
  }

#define EXPECT_EQ_AREA(expected, actual)                             \
  {                                                                  \
    EXPECT_EQ(expected.snap_axis, actual.snap_axis);                 \
    EXPECT_EQ(expected.snap_position.x(), actual.snap_position.x()); \
    EXPECT_EQ(expected.snap_position.y(), actual.snap_position.y()); \
    EXPECT_EQ(expected.must_snap, actual.must_snap);                 \
  }

// The following tests check the snap data are correctly calculated.
TEST_P(SnapCoordinatorTest, StartAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.left - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_x = (200 - 8) - 10;
  // (#area.top - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_y = (200 - 8) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, ScrolledStartAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* scroller_element = GetDocument().getElementById("scroller");
  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  scroller_element->scrollBy(20, 20);
  EXPECT_EQ(FloatPoint(20, 20), scrollable_area->ScrollPosition());
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.left - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_x = (200 - 8) - 10;
  // (#area.top - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_y = (200 - 8) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, StartAlignmentCalculationWithBoxModel) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             "scroll-snap-align: start; margin: 2px; border: "
                             "9px solid; padding: 5px;");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(
      styleAttr, "margin: 3px; border: 10px solid; padding: 4px;");
  GetDocument().UpdateStyleAndLayout();
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#scroller.padding + #area.left + #area.margin - #area.scroll-margin)
  //  - (#scroller.scroll-padding)
  double snap_position_x = (4 + 200 + 2 - 8) - 10;
  // (#scroller.padding + #area.top + #area.margin - #area.scroll-margin)
  //  - (#scroller.scroll-padding)
  double snap_position_y = (4 + 200 + 2 - 8) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, NegativeMarginStartAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             "scroll-snap-align: start; scroll-margin: -8px;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.left - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_x = (200 - (-8)) - 10;
  // (#area.top - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_y = (200 - (-8)) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, CenterAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: center;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.left + #area.right) / 2 - #scroller.width / 2
  double snap_position_x =
      (200 + (200 + 100)) / 2 - float(scroller_element->clientWidth()) / 2;
  // (#area.top + #area.bottom) / 2 - #scroller.height / 2
  double snap_position_y =
      (200 + (200 + 100)) / 2 - float(scroller_element->clientHeight()) / 2;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, AsymmetricalCenterAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             R"HTML(
        scroll-snap-align: center;
        scroll-margin-top: 2px;
        scroll-margin-right: 4px;
        scroll-margin-bottom: 6px;
        scroll-margin-left: 8px;
      )HTML");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(styleAttr,
                                 R"HTML(
        scroll-padding-top: 10px;
        scroll-padding-right: 12px;
        scroll-padding-bottom: 14px;
        scroll-padding-left: 16px;
      )HTML");
  GetDocument().UpdateStyleAndLayout();
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.left - #area.scroll-margin-left +
  //  #area.right + #area.scroll-margin-right) / 2 -
  // (#scroller.left + #scroller.scroll-padding-left +
  //  #scroller.right - #scroller.scroll-padding-right) / 2
  double snap_position_x =
      (200 - 8 + (200 + 100 + 4)) / 2 -
      (0 + 16 + float(scroller_element->clientWidth()) - 12) / 2;

  // (#area.top - #area.scroll-margin-top +
  //  #area.bottom + #area.scroll-margin-bottom) / 2 -
  // (#scroller.top + #scroller.scroll-padding-top +
  //  #scroller.bottom - #scroller.scroll-padding-bottom) / 2
  double snap_position_y =
      (200 - 2 + (200 + 100 + 6)) / 2 -
      (0 + 10 + float(scroller_element->clientHeight()) - 14) / 2;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, EndAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: end;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.right + #area.scroll-margin)
  // - (#scroller.right - #scroller.scroll-padding)
  double snap_position_x =
      (200 + 100 + 8) - (scroller_element->clientWidth() - 10);

  // (#area.bottom + #area.scroll-margin)
  // - (#scroller.bottom - #scroller.scroll-padding)
  double snap_position_y =
      (200 + 100 + 8) - (scroller_element->clientHeight() - 10);

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, EndAlignmentCalculationWithBoxModel) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(
      styleAttr,
      "scroll-snap-align: end; margin: 2px; border: 9px solid; padding: 5px;");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(
      styleAttr, "margin: 3px; border: 10px solid; padding: 4px;");
  GetDocument().UpdateStyleAndLayout();
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#scroller.padding + #area.left + #area.margin + #area.width
  //   + 2 x (#area.border + #area.padding)) + #area.scroll-margin)
  //  - (#scroller.width - #scroller.scroll-padding)
  double snap_position_x = (4 + 200 + 2 + 100 + 2 * (9 + 5) + 8) -
                           (scroller_element->clientWidth() - 10);
  // (#scroller.padding + #area.top + #area.height + #area.margin
  //   + 2 x (#area.border + #area.padding)) + #area.scroll-margin)
  //  - (#scroller.height - #scroller.scroll-padding)
  double snap_position_y = (4 + 200 + 2 + 100 + 2 * (9 + 5) + 8) -
                           (scroller_element->clientHeight() - 10);

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, ScaledEndAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             "scroll-snap-align: end; transform: scale(4, 4);");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // The area is scaled from center, so it pushes the area's top-left corner to
  // (50, 50).
  // (#area.right + #area.scroll-margin)
  // - (#scroller.right - #scroller.scroll-padding)
  double snap_position_x =
      (50 + 400 + 8) - (scroller_element->clientWidth() - 10);

  // (#area.bottom + #area.scroll-margin)
  // - (#scroller.bottom - #scroller.scroll-padding)
  double snap_position_y =
      (50 + 400 + 8) - (scroller_element->clientHeight() - 10);

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, VerticalRlStartAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             "scroll-snap-align: start; left: -200px;");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(styleAttr, "writing-mode: vertical-rl;");
  GetDocument().UpdateStyleAndLayout();
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // Under vertical-rl writing mode, 'start' should align to the right.
  // (#area.right + #area.scroll-margin)
  // - (#scroller.right - #scroller.scroll-padding)
  double snap_position_x =
      (200 + 100 + 8) - (scroller_element->clientWidth() - 10);

  // (#area.top - #area.scroll-margin) - (#scroller.scroll-padding)
  double snap_position_y = (200 - 8) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_P(SnapCoordinatorTest, OverflowedSnapPositionCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             "left: 0px; top: 0px; scroll-snap-align: end;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  Optional<SnapContainerData> data =
      snap_coordinator->GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data.has_value());
  SnapContainerData actual_container = data.value();

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset(), scrollable_area->ScrollOrigin());

  // (#area.right + #area.scroll-margin)
  //  - (#scroller.right - #scroller.scroll-padding)
  // = (100 + 8) - (clientWidth - 10) < 0
  // As scrollPosition cannot be set to a negative number, we set it to 0.
  double snap_position_x = 0;

  // (#area.bottom + #area.scroll-margin)
  //  - (#scroller.bottom - #scroller.scroll-padding)
  // = (100 + 8) - (clientHeight - 10) < 0
  // As scrollPosition cannot be set to a negative number, we set it to 0.
  double snap_position_y = 0;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  SnapAreaData expected_area(
      SnapAxis::kBoth, gfx::ScrollOffset(snap_position_x, snap_position_y),
      must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

// The following tests check GetSnapPosition().
TEST_P(SnapCoordinatorTest, SnapsIfScrolledAndSnappingAxesMatch) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  Element* scroller_element = GetDocument().getElementById("scroller");
  area_element->setAttribute(styleAttr, "scroll-snap-align: start;");
  scroller_element->setAttribute(styleAttr, "scroll-snap-type: x mandatory");
  GetDocument().UpdateStyleAndLayout();

  scroller_element->setScrollLeft(150);
  scroller_element->setScrollTop(150);

  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  LayoutBox* snap_container = scroller_element->GetLayoutBox();
  FloatPoint snap_position;
  EXPECT_TRUE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                &snap_position));
  EXPECT_EQ(200 - 8 - 10, snap_position.X());
  EXPECT_EQ(150, snap_position.Y());
}

TEST_P(SnapCoordinatorTest, DoesNotSnapOnNonSnappingAxis) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  Element* scroller_element = GetDocument().getElementById("scroller");
  area_element->setAttribute(styleAttr, "scroll-snap-align: start;");
  scroller_element->setAttribute(styleAttr, "scroll-snap-type: y mandatory");
  GetDocument().UpdateStyleAndLayout();

  scroller_element->setScrollLeft(150);
  scroller_element->setScrollTop(150);

  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  LayoutBox* snap_container = scroller_element->GetLayoutBox();
  FloatPoint snap_position;
  EXPECT_FALSE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                 &snap_position));
}

TEST_P(SnapCoordinatorTest, DoesNotSnapOnEmptyContainer) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  Element* scroller_element = GetDocument().getElementById("scroller");
  area_element->setAttribute(styleAttr, "scroll-snap-align: none;");
  scroller_element->setAttribute(styleAttr, "scroll-snap-type: x mandatory");
  GetDocument().UpdateStyleAndLayout();

  scroller_element->setScrollLeft(150);
  scroller_element->setScrollTop(150);

  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  LayoutBox* snap_container = scroller_element->GetLayoutBox();
  FloatPoint snap_position;
  EXPECT_FALSE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                 &snap_position));
}

TEST_P(SnapCoordinatorTest, DoesNotSnapOnNonSnapContainer) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  Element* scroller_element = GetDocument().getElementById("scroller");
  area_element->setAttribute(styleAttr, "scroll-snap-align: start;");
  scroller_element->setAttribute(styleAttr, "scroll-snap-type: none");
  GetDocument().UpdateStyleAndLayout();

  scroller_element->setScrollLeft(150);
  scroller_element->setScrollTop(150);

  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  LayoutBox* snap_container = scroller_element->GetLayoutBox();
  FloatPoint snap_position;
  EXPECT_FALSE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                 &snap_position));
}

}  // namespace
