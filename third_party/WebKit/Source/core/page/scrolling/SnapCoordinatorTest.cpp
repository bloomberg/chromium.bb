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
        scroll-snap-margin: 8px;
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

  // When body is viewport defining and overflows then any snap points on the
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

#define EXPECT_EQ_CONTAINER(expected, actual)                                \
  {                                                                          \
    EXPECT_EQ(expected.min_offset.Width(), actual.min_offset.Width());       \
    EXPECT_EQ(expected.min_offset.Height(), actual.min_offset.Height());     \
    EXPECT_EQ(expected.max_offset.Width(), actual.max_offset.Width());       \
    EXPECT_EQ(expected.max_offset.Height(), actual.max_offset.Height());     \
    EXPECT_EQ(expected.scroll_snap_type, actual.scroll_snap_type);           \
    EXPECT_EQ(expected.snap_area_list.size(), actual.snap_area_list.size()); \
  }

#define EXPECT_EQ_AREA(expected, actual)                                   \
  {                                                                        \
    EXPECT_EQ(expected.snap_axis, actual.snap_axis);                       \
    EXPECT_EQ(expected.snap_offset.Width(), actual.snap_offset.Width());   \
    EXPECT_EQ(expected.snap_offset.Height(), actual.snap_offset.Height()); \
    EXPECT_EQ(expected.must_snap, actual.must_snap);                       \
  }

// The following tests check EnsureSnapContainerData().
TEST_P(SnapCoordinatorTest, StartAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: start;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  SnapContainerData actual_container =
      snap_coordinator->EnsureSnapContainerData(
          *scroller_element->GetLayoutBox());

  double scrollbar_width = scroller_element->GetLayoutBox()
                               ->GetScrollableArea()
                               ->VerticalScrollbar()
                               ->ScrollbarThickness();
  double scrollbar_height = scroller_element->GetLayoutBox()
                                ->GetScrollableArea()
                                ->HorizontalScrollbar()
                                ->ScrollbarThickness();
  // #container.width - (#scroller.width - scrollbar_width)
  double scrollable_x = 500 - (140 - scrollbar_width);
  // #container.height - (#scroller.height - scrollbar.height)
  double scrollable_y = 500 - (160 - scrollbar_height);

  // (#area.left - #area.scroll-snap-margin) - (#scroller.scroll-padding)
  double snap_offset_x = (200 - 8) - 10;
  // (#area.top - #area.scroll-snap-margin) - (#scroller.scroll-padding)
  double snap_offset_y = (200 - 8) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(scrollable_x, scrollable_y));
  SnapAreaData expected_area(
      SnapAxis::kBoth, ScrollOffset(snap_offset_x, snap_offset_y), must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.snap_area_list[0]);
}

TEST_P(SnapCoordinatorTest, NegativeMarginStartAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(
      styleAttr, "scroll-snap-align: start; scroll-snap-margin: -8px;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  SnapContainerData actual_container =
      snap_coordinator->EnsureSnapContainerData(
          *scroller_element->GetLayoutBox());

  double scrollbar_width = scroller_element->GetLayoutBox()
                               ->GetScrollableArea()
                               ->VerticalScrollbar()
                               ->ScrollbarThickness();
  double scrollbar_height = scroller_element->GetLayoutBox()
                                ->GetScrollableArea()
                                ->HorizontalScrollbar()
                                ->ScrollbarThickness();
  // #container.width - (#scroller.width - scrollbar_width)
  double scrollable_x = 500 - (140 - scrollbar_width);
  // #container.height - (#scroller.height - scrollbar.height)
  double scrollable_y = 500 - (160 - scrollbar_height);

  // (#area.left - #area.scroll-snap-margin) - (#scroller.scroll-padding)
  double snap_offset_x = (200 - (-8)) - 10;
  // (#area.top - #area.scroll-snap-margin) - (#scroller.scroll-padding)
  double snap_offset_y = (200 - (-8)) - 10;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(scrollable_x, scrollable_y));
  SnapAreaData expected_area(
      SnapAxis::kBoth, ScrollOffset(snap_offset_x, snap_offset_y), must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.snap_area_list[0]);
}

TEST_P(SnapCoordinatorTest, CenterAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: center;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  SnapContainerData actual_container =
      snap_coordinator->EnsureSnapContainerData(
          *scroller_element->GetLayoutBox());

  double scrollbar_width = scroller_element->GetLayoutBox()
                               ->GetScrollableArea()
                               ->VerticalScrollbar()
                               ->ScrollbarThickness();
  double scrollbar_height = scroller_element->GetLayoutBox()
                                ->GetScrollableArea()
                                ->HorizontalScrollbar()
                                ->ScrollbarThickness();
  // #container.width - (#scroller.width - scrollbar_width)
  double scrollable_x = 500 - (140 - scrollbar_width);
  // #container.height - (#scroller.height - scrollbar.height)
  double scrollable_y = 500 - (160 - scrollbar_height);

  // (#area.left + #area.right) / 2 - (#scroller.left + #scroller.right) / 2
  double snap_offset_x = (200 + (200 + 100)) / 2 - 140 / 2;
  // (#area.top + #area.bottom) / 2 - (#scroller.top + #scroller.bottom) / 2
  double snap_offset_y = (200 + (200 + 100)) / 2 - 160 / 2;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(scrollable_x, scrollable_y));
  SnapAreaData expected_area(
      SnapAxis::kBoth, ScrollOffset(snap_offset_x, snap_offset_y), must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.snap_area_list[0]);
}

TEST_P(SnapCoordinatorTest, AsymmetricalCenterAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             R"HTML(
        scroll-snap-align: center;
        scroll-snap-margin-top: 2px;
        scroll-snap-margin-right: 4px;
        scroll-snap-margin-bottom: 6px;
        scroll-snap-margin-left: 8px;
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
  SnapContainerData actual_container =
      snap_coordinator->EnsureSnapContainerData(
          *scroller_element->GetLayoutBox());

  double scrollbar_width = scroller_element->GetLayoutBox()
                               ->GetScrollableArea()
                               ->VerticalScrollbar()
                               ->ScrollbarThickness();
  double scrollbar_height = scroller_element->GetLayoutBox()
                                ->GetScrollableArea()
                                ->HorizontalScrollbar()
                                ->ScrollbarThickness();
  // #container.width - (#scroller.width - scrollbar_width)
  double scrollable_x = 500 - (140 - scrollbar_width);
  // #container.height - (#scroller.height - scrollbar.height)
  double scrollable_y = 500 - (160 - scrollbar_height);

  // (#area.left - #area.scroll-snap-margin-left +
  //  #area.right + #area.scroll-snap-margin-right) / 2 -
  // (#scroller.left + #scroller.scroll-padding-left +
  //  #scroller.right - #scroller.scroll-padding-right) / 2
  double snap_offset_x =
      (200 - 8 + (200 + 100 + 4)) / 2 - (0 + 16 + 140 - 12) / 2;

  // (#area.top - #area.scroll-snap-margin-top +
  //  #area.bottom + #area.scroll-snap-margin-bottom) / 2 -
  // (#scroller.top + #scroller.scroll-padding-top +
  //  #scroller.bottom - #scroller.scroll-padding-bottom) / 2
  double snap_offset_y =
      (200 - 2 + (200 + 100 + 6)) / 2 - (0 + 10 + 160 - 14) / 2;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(scrollable_x, scrollable_y));
  SnapAreaData expected_area(
      SnapAxis::kBoth, ScrollOffset(snap_offset_x, snap_offset_y), must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.snap_area_list[0]);
}

TEST_P(SnapCoordinatorTest, EndAlignmentCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr, "scroll-snap-align: end;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  SnapContainerData actual_container =
      snap_coordinator->EnsureSnapContainerData(
          *scroller_element->GetLayoutBox());

  double scrollbar_width = scroller_element->GetLayoutBox()
                               ->GetScrollableArea()
                               ->VerticalScrollbar()
                               ->ScrollbarThickness();
  double scrollbar_height = scroller_element->GetLayoutBox()
                                ->GetScrollableArea()
                                ->HorizontalScrollbar()
                                ->ScrollbarThickness();
  // #container.width - (#scroller.width - scrollbar_width)
  double scrollable_x = 500 - (140 - scrollbar_width);
  // #container.height - (#scroller.height - scrollbar.height)
  double scrollable_y = 500 - (160 - scrollbar_height);

  // (#area.right + #area.scroll-snap-margin)
  // - (#scroller.right - #scroller.scroll-padding)
  double snap_offset_x = (200 + 100 + 8) - (140 - 10);

  // (#area.bottom + #area.scroll-snap-margin)
  // - (#scroller.bottom - #scroller.scroll-padding)
  double snap_offset_y = (200 + 100 + 8) - (160 - 10);

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(scrollable_x, scrollable_y));
  SnapAreaData expected_area(
      SnapAxis::kBoth, ScrollOffset(snap_offset_x, snap_offset_y), must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.snap_area_list[0]);
}

TEST_P(SnapCoordinatorTest, OverflowedSnapPositionCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(styleAttr,
                             "left: 0px; top: 0px; scroll-snap-align: end;");
  GetDocument().UpdateStyleAndLayout();
  Element* scroller_element = GetDocument().getElementById("scroller");
  SnapCoordinator* snap_coordinator = GetDocument().GetSnapCoordinator();
  SnapContainerData actual_container =
      snap_coordinator->EnsureSnapContainerData(
          *scroller_element->GetLayoutBox());

  double scrollbar_width = scroller_element->GetLayoutBox()
                               ->GetScrollableArea()
                               ->VerticalScrollbar()
                               ->ScrollbarThickness();
  double scrollbar_height = scroller_element->GetLayoutBox()
                                ->GetScrollableArea()
                                ->HorizontalScrollbar()
                                ->ScrollbarThickness();
  // #container.width - (#scroller.width - scrollbar_width)
  double scrollable_x = 500 - (140 - scrollbar_width);
  // #container.height - (#scroller.height - scrollbar.height)
  double scrollable_y = 500 - (160 - scrollbar_height);

  // (#area.right + #area.scroll-snap-margin)
  //  - (#scroller.right - #scroller.scroll-padding)
  // = (100 + 8) - (140 - 10)
  // As scrollOffset cannot be set to -22, we set it to 0.
  double snap_offset_x = 0;

  // (#area.bottom + #area.scroll-snap-margin)
  //  - (#scroller.bottom - #scroller.scroll-padding)
  // = (100 + 8) - (160 - 10)
  // As scrollOffset cannot be set to -42, we set it to 0.
  double snap_offset_y = 0;

  bool must_snap = false;

  SnapContainerData expected_container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(scrollable_x, scrollable_y));
  SnapAreaData expected_area(
      SnapAxis::kBoth, ScrollOffset(snap_offset_x, snap_offset_y), must_snap);
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.snap_area_list[0]);
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
  ScrollOffset snap_offset;
  EXPECT_TRUE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                &snap_offset));
  EXPECT_EQ(200 - 8 - 10, snap_offset.Width());
  EXPECT_EQ(150, snap_offset.Height());
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
  ScrollOffset snap_offset;
  EXPECT_FALSE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                 &snap_offset));
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
  ScrollOffset snap_offset;
  EXPECT_FALSE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                 &snap_offset));
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
  ScrollOffset snap_offset;
  EXPECT_FALSE(snap_coordinator->GetSnapPosition(*snap_container, true, false,
                                                 &snap_offset));
}

// The following tests check FindSnapOffset().
TEST_P(SnapCoordinatorTest, FindsClosestSnapOffsetIndependently) {
  SnapContainerData container_data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(360, 380));
  ScrollOffset current_offset(100, 100);
  SnapAreaData snap_x_only(SnapAxis::kX,
                           ScrollOffset(80, SnapAreaData::kInvalidScrollOffset),
                           false);
  SnapAreaData snap_y_only(SnapAxis::kY,
                           ScrollOffset(SnapAreaData::kInvalidScrollOffset, 70),
                           false);
  SnapAreaData snap_on_both(SnapAxis::kBoth, ScrollOffset(50, 150), false);
  container_data.AddSnapAreaData(snap_x_only);
  container_data.AddSnapAreaData(snap_y_only);
  container_data.AddSnapAreaData(snap_on_both);
  ScrollOffset snapped_offset = SnapCoordinator::FindSnapOffset(
      current_offset, container_data, true, true);
  EXPECT_EQ(80, snapped_offset.Width());
  EXPECT_EQ(70, snapped_offset.Height());
}

TEST_P(SnapCoordinatorTest, FindsClosestSnapOffsetOnAxisValueBoth) {
  SnapContainerData container_data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(360, 380));
  ScrollOffset current_offset(40, 150);
  SnapAreaData snap_x_only(SnapAxis::kX,
                           ScrollOffset(80, SnapAreaData::kInvalidScrollOffset),
                           false);
  SnapAreaData snap_y_only(SnapAxis::kY,
                           ScrollOffset(SnapAreaData::kInvalidScrollOffset, 70),
                           false);
  SnapAreaData snap_on_both(SnapAxis::kBoth, ScrollOffset(50, 150), false);
  container_data.AddSnapAreaData(snap_x_only);
  container_data.AddSnapAreaData(snap_y_only);
  container_data.AddSnapAreaData(snap_on_both);
  ScrollOffset snapped_offset = SnapCoordinator::FindSnapOffset(
      current_offset, container_data, true, true);
  EXPECT_EQ(50, snapped_offset.Width());
  EXPECT_EQ(150, snapped_offset.Height());
}

TEST_P(SnapCoordinatorTest, DoesNotSnapOnNonScrolledAxis) {
  SnapContainerData container_data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      ScrollOffset(), ScrollOffset(360, 380));
  ScrollOffset current_offset(100, 100);
  SnapAreaData snap_x_only(SnapAxis::kX,
                           ScrollOffset(80, SnapAreaData::kInvalidScrollOffset),
                           false);
  SnapAreaData snap_y_only(SnapAxis::kY,
                           ScrollOffset(SnapAreaData::kInvalidScrollOffset, 70),
                           false);
  container_data.AddSnapAreaData(snap_x_only);
  container_data.AddSnapAreaData(snap_y_only);
  ScrollOffset snapped_offset = SnapCoordinator::FindSnapOffset(
      current_offset, container_data, true, false);
  EXPECT_EQ(80, snapped_offset.Width());
  EXPECT_EQ(100, snapped_offset.Height());
}

}  // namespace
