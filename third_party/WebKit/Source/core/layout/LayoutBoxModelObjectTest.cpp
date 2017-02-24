// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBoxModelObject.h"

#include "core/dom/DOMTokenList.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/html/HTMLElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/page/scrolling/StickyPositionScrollingConstraints.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutBoxModelObjectTest : public RenderingTest {
 protected:
  const FloatRect& getScrollContainerRelativeContainingBlockRect(
      const StickyPositionScrollingConstraints& constraints) const {
    return constraints.scrollContainerRelativeContainingBlockRect();
  }

  const FloatRect& getScrollContainerRelativeStickyBoxRect(
      const StickyPositionScrollingConstraints& constraints) const {
    return constraints.scrollContainerRelativeStickyBoxRect();
  }
};

// Verifies that the sticky constraints are correctly computed.
TEST_F(LayoutBoxModelObjectTest, StickyPositionConstraints) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "}"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; }"
      "#scroller { width: 400px; height: 100px; overflow: auto; "
      "position: relative; top: 200px; border: 2px solid black; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollOffsetInt().width(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(15, 115, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(IntRect(0, 50, 400, 100),
            enclosingIntRect(sticky->computeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are correctly computed in right to left.
TEST_F(LayoutBoxModelObjectTest, StickyPositionVerticalRLConstraints) {
  setBodyInnerHTML(
      "<style> html { -webkit-writing-mode: vertical-rl; } "
      "#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "}"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; }"
      "#scroller { width: 400px; height: 100px; overflow: auto; "
      "position: relative; top: 200px; border: 2px solid black; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollOffsetInt().width(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(215, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(285, 115, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(IntRect(0, 50, 400, 100),
            enclosingIntRect(sticky->computeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are not affected by transforms
TEST_F(LayoutBoxModelObjectTest, StickyPositionTransforms) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "transform: scale(2); transform-origin: top left; }"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; "
      "transform: scale(2); transform-origin: top left; }"
      "#scroller { height: 100px; overflow: auto; position: relative; top: "
      "200px; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollOffsetInt().width(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(15, 115, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Verifies that the sticky constraints are correctly computed.
TEST_F(LayoutBoxModelObjectTest, StickyPositionPercentageStyles) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; margin-top: 10%; top: 0; width: "
      "100px; height: 100px; }"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 250px; padding: 5%; border: 5px solid black; }"
      "#scroller { width: 400px; height: 100px; overflow: auto; position: "
      "relative; top: 200px; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  ASSERT_EQ(IntRect(25, 145, 200, 330),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(25, 145, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Verifies that the sticky constraints are correct when the sticky position
// container is also the ancestor scroller.
TEST_F(LayoutBoxModelObjectTest, StickyPositionContainerIsScroller) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "}"
      "#scroller { height: 100px; width: 400px; overflow: auto; position: "
      "relative; top: 200px; border: 2px solid black; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='sticky'></div><div "
      "class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  ASSERT_EQ(IntRect(0, 0, 400, 1100),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(0, 0, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Verifies that the sticky constraints are correct when the sticky position
// object has an anonymous containing block.
TEST_F(LayoutBoxModelObjectTest, StickyPositionAnonymousContainer) {
  setBodyInnerHTML(
      "<style>#sticky { display: inline-block; position: sticky; top: 0; "
      "width: 100px; height: 100px; }"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; }"
      "#scroller { height: 100px; overflow: auto; position: relative; top: "
      "200px; }"
      ".header { height: 50px; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div class='header'></div><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(15, 165, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

TEST_F(LayoutBoxModelObjectTest, StickyPositionTableContainers) {
  setBodyInnerHTML(
      "<style> td, th { height: 50px; width: 50px; } "
      "#sticky { position: sticky; left: 0; will-change: transform; }"
      "table {border: none; }"
      "#scroller { overflow: auto; }"
      "</style>"
      "<div id='scroller'>"
      "<table cellspacing='0' cellpadding='0'>"
      "    <thead><tr><td></td></tr></thead>"
      "    <tr><td id='sticky'></td></tr>"
      "</table></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().at(sticky->layer());
  EXPECT_EQ(IntRect(0, 0, 50, 100),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  EXPECT_EQ(
      IntRect(0, 50, 50, 50),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Tests that when a non-layer changes size it invalidates the constraints for
// sticky position elements within the same scroller.
TEST_F(LayoutBoxModelObjectTest, StickyPositionConstraintInvalidation) {
  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: auto; display: flex; width: 200px; }"
      "#target { width: 50px; }"
      "#sticky { position: sticky; top: 0; }"
      ".container { width: 100px; margin-left: auto; margin-right: auto; }"
      ".hide { display: none; }"
      "</style>"
      "<div id='scroller'>"
      "  <div style='flex: 1'>"
      "    <div class='container'><div id='sticky'></div>"
      "  </div>"
      "</div>"
      "<div class='spacer' id='target'></div>"
      "</div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  LayoutBoxModelObject* target =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"));
  EXPECT_TRUE(scrollableArea->stickyConstraintsMap().contains(sticky->layer()));
  EXPECT_EQ(25.f,
            getScrollContainerRelativeStickyBoxRect(
                scrollableArea->stickyConstraintsMap().at(sticky->layer()))
                .location()
                .x());
  toHTMLElement(target->node())->classList().add("hide", ASSERT_NO_EXCEPTION);
  document().view()->updateLifecycleToLayoutClean();
  // Layout should invalidate the sticky constraints of the sticky element and
  // mark it as needing a compositing inputs update.
  EXPECT_FALSE(
      scrollableArea->stickyConstraintsMap().contains(sticky->layer()));
  EXPECT_TRUE(sticky->layer()->needsCompositingInputsUpdate());

  // After updating compositing inputs we should have the updated position.
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(50.f,
            getScrollContainerRelativeStickyBoxRect(
                scrollableArea->stickyConstraintsMap().at(sticky->layer()))
                .location()
                .x());
}

// Verifies that the correct sticky-box shifting ancestor is found when
// computing the sticky constraints. Any such ancestor is the first sticky
// element between you and your containing block (exclusive).
//
// In most cases, this pointer should be null since your parent is normally your
// containing block. However there are cases where this is not true, including
// inline blocks and tables. The latter is currently irrelevant since only table
// cells can be sticky in CSS2.1, but we can test the former.
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectStickyBoxShiftingAncestor) {
  setBodyInnerHTML(
      "<style>#stickyOuterDiv { position: sticky; }"
      "#stickyOuterInline { position: sticky; display: inline; }"
      "#stickyInnerInline { position: sticky; display: inline; }</style>"
      "<div id='stickyOuterDiv'><div id='stickyOuterInline'>"
      "<div id='stickyInnerInline'></div></div></div>");

  LayoutBoxModelObject* stickyOuterDiv =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyOuterDiv"));
  LayoutBoxModelObject* stickyOuterInline =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyOuterInline"));
  LayoutBoxModelObject* stickyInnerInline =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyInnerInline"));

  PaintLayerScrollableArea* scrollableArea =
      stickyOuterDiv->layer()->ancestorOverflowLayer()->getScrollableArea();
  ASSERT_TRUE(scrollableArea);
  StickyConstraintsMap constraintsMap = scrollableArea->stickyConstraintsMap();

  ASSERT_TRUE(constraintsMap.contains(stickyOuterDiv->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyOuterInline->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyInnerInline->layer()));

  // The outer block element trivially has no sticky-box shifting ancestor.
  EXPECT_FALSE(constraintsMap.at(stickyOuterDiv->layer())
                   .nearestStickyBoxShiftingStickyBox());

  // Neither does the outer inline element, as its parent element is also its
  // containing block.
  EXPECT_FALSE(constraintsMap.at(stickyOuterInline->layer())
                   .nearestStickyBoxShiftingStickyBox());

  // However the inner inline element does have a sticky-box shifting ancestor,
  // as its containing block is the ancestor block element, not its parent.
  EXPECT_EQ(stickyOuterInline,
            constraintsMap.at(stickyInnerInline->layer())
                .nearestStickyBoxShiftingStickyBox());
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints. Any such ancestor is the first sticky
// element between your containing block (inclusive) and your ancestor overflow
// layer (exclusive).
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestor) {
  // We make the scroller itself sticky in order to check that elements do not
  // detect it as their containing-block shifting ancestor.
  setBodyInnerHTML(
      "<style>#scroller { overflow-y: scroll; position: sticky; }"
      "#stickyParent { position: sticky; }"
      "#stickyChild { position: sticky; }"
      "#stickyNestedChild { position: sticky; }</style>"
      "<div id='scroller'><div id='stickyParent'><div id='stickyChild'></div>"
      "<div><div id='stickyNestedChild'></div></div></div></div>");

  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  LayoutBoxModelObject* stickyParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyParent"));
  LayoutBoxModelObject* stickyChild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyChild"));
  LayoutBoxModelObject* stickyNestedChild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyNestedChild"));

  PaintLayerScrollableArea* scrollableArea =
      scroller->layer()->getScrollableArea();
  ASSERT_TRUE(scrollableArea);
  StickyConstraintsMap constraintsMap = scrollableArea->stickyConstraintsMap();

  ASSERT_FALSE(constraintsMap.contains(scroller->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyParent->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyChild->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyNestedChild->layer()));

  // The outer <div> should not detect the scroller as its containing-block
  // shifting ancestor.
  EXPECT_FALSE(constraintsMap.at(stickyParent->layer())
                   .nearestStickyBoxShiftingContainingBlock());

  // Both inner children should detect the parent <div> as their
  // containing-block shifting ancestor.
  EXPECT_EQ(stickyParent,
            constraintsMap.at(stickyChild->layer())
                .nearestStickyBoxShiftingContainingBlock());
  EXPECT_EQ(stickyParent,
            constraintsMap.at(stickyNestedChild->layer())
                .nearestStickyBoxShiftingContainingBlock());
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints, in the case where the overflow ancestor is
// the page itself. This is a special-case version of the test above, as we
// often treat the root page as special when it comes to scroll logic. It should
// not make a difference for containing-block shifting ancestor calculations.
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestorRoot) {
  setBodyInnerHTML(
      "<style>#stickyParent { position: sticky; }"
      "#stickyGrandchild { position: sticky; }</style>"
      "<div id='stickyParent'><div><div id='stickyGrandchild'></div></div>"
      "</div>");

  LayoutBoxModelObject* stickyParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyParent"));
  LayoutBoxModelObject* stickyGrandchild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyGrandchild"));

  PaintLayerScrollableArea* scrollableArea =
      stickyParent->layer()->ancestorOverflowLayer()->getScrollableArea();
  ASSERT_TRUE(scrollableArea);
  StickyConstraintsMap constraintsMap = scrollableArea->stickyConstraintsMap();

  ASSERT_TRUE(constraintsMap.contains(stickyParent->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyGrandchild->layer()));

  // The grandchild sticky should detect the parent as its containing-block
  // shifting ancestor.
  EXPECT_EQ(stickyParent,
            constraintsMap.at(stickyGrandchild->layer())
                .nearestStickyBoxShiftingContainingBlock());
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints, in the case of tables. Tables are unusual
// because the containing block for all table elements is the <table> itself, so
// we have to skip over elements to find the correct ancestor.
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestorTable) {
  setBodyInnerHTML(
      "<style>#scroller { overflow-y: scroll; }"
      "#stickyOuter { position: sticky; }"
      "#stickyTh { position: sticky; }</style>"
      "<div id='scroller'><div id='stickyOuter'><table><thead><tr>"
      "<th id='stickyTh'></th></tr></thead></table></div></div>");

  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  LayoutBoxModelObject* stickyOuter =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyOuter"));
  LayoutBoxModelObject* stickyTh =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyTh"));

  PaintLayerScrollableArea* scrollableArea =
      scroller->layer()->getScrollableArea();
  ASSERT_TRUE(scrollableArea);
  StickyConstraintsMap constraintsMap = scrollableArea->stickyConstraintsMap();

  ASSERT_FALSE(constraintsMap.contains(scroller->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyOuter->layer()));
  ASSERT_TRUE(constraintsMap.contains(stickyTh->layer()));

  // The table cell should detect the outer <div> as its containing-block
  // shifting ancestor.
  EXPECT_EQ(stickyOuter,
            constraintsMap.at(stickyTh->layer())
                .nearestStickyBoxShiftingContainingBlock());
}

// Verifies that the calculated position:sticky offsets are correct when we have
// a simple case of nested sticky elements.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNested) {
  setBodyInnerHTML(
      "<style>#scroller { height: 100px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px }"
      "#stickyParent { position: sticky; top: 0; height: 50px; }"
      "#stickyChild { position: sticky; top: 0; height: 25px; }"
      "#postPadding { height: 200px }</style>"
      "<div id='scroller'><div id='prePadding'></div><div id='stickyParent'>"
      "<div id='stickyChild'></div></div><div id='postPadding'></div></div>");

  LayoutBoxModelObject* stickyParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyParent"));
  LayoutBoxModelObject* stickyChild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyChild"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollableArea->scrollPosition().y());

  // Both the parent and child sticky divs are attempting to place themselves at
  // the top of the scrollable area. To achieve this the parent must offset on
  // the y-axis against its starting position. The child is offset relative to
  // its parent so should not move at all.
  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), stickyChild->stickyPositionOffset());

  stickyParent->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());

  stickyChild->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), stickyChild->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a larger edge constraint value than the parent.
TEST_F(LayoutBoxModelObjectTest, StickyPositionChildHasLargerTop) {
  setBodyInnerHTML(
      "<style>#scroller { height: 100px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px }"
      "#stickyParent { position: sticky; top: 0; height: 50px; }"
      "#stickyChild { position: sticky; top: 25px; height: 25px; }"
      "#postPadding { height: 200px }</style>"
      "<div id='scroller'><div id='prePadding'></div><div id='stickyParent'>"
      "<div id='stickyChild'></div></div><div id='postPadding'></div></div>");

  LayoutBoxModelObject* stickyParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyParent"));
  LayoutBoxModelObject* stickyChild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyChild"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollableArea->scrollPosition().y());

  // The parent is attempting to place itself at the top of the scrollable area,
  // whilst the child is attempting to be 25 pixels from the top. To achieve
  // this both must offset on the y-axis against their starting positions, but
  // note the child is offset relative to the parent.
  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), stickyChild->stickyPositionOffset());

  stickyParent->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());

  stickyChild->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), stickyChild->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a smaller edge constraint value than the parent.
TEST_F(LayoutBoxModelObjectTest, StickyPositionParentHasLargerTop) {
  setBodyInnerHTML(
      "<style>#scroller { height: 100px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px }"
      "#stickyParent { position: sticky; top: 25px; height: 50px; }"
      "#stickyChild { position: sticky; top: 0; height: 25px; }"
      "#postPadding { height: 200px }</style>"
      "<div id='scroller'><div id='prePadding'></div><div id='stickyParent'>"
      "<div id='stickyChild'></div></div><div id='postPadding'></div></div>");

  LayoutBoxModelObject* stickyParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyParent"));
  LayoutBoxModelObject* stickyChild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyChild"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollableArea->scrollPosition().y());

  // The parent is attempting to place itself 25 pixels from the top of the
  // scrollable area, whilst the child is attempting to be at the top. However,
  // the child must stay contained within the parent, so it should be pushed
  // down to the same height. As always, the child offset is relative.
  EXPECT_EQ(LayoutSize(0, 75), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), stickyChild->stickyPositionOffset());

  stickyParent->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 75), stickyParent->stickyPositionOffset());

  stickyChild->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 75), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), stickyChild->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a large enough edge constraint value to push outside of its parent.
TEST_F(LayoutBoxModelObjectTest, StickyPositionChildPushingOutsideParent) {
  setBodyInnerHTML(
      "<style> #scroller { height: 100px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px; }"
      "#stickyParent { position: sticky; top: 0; height: 50px; }"
      "#stickyChild { position: sticky; top: 50px; height: 25px; }"
      "#postPadding { height: 200px }</style>"
      "<div id='scroller'><div id='prePadding'></div><div id='stickyParent'>"
      "<div id='stickyChild'></div></div><div id='postPadding'></div></div>");

  LayoutBoxModelObject* stickyParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyParent"));
  LayoutBoxModelObject* stickyChild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyChild"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollableArea->scrollPosition().y());

  // The parent is attempting to place itself at the top of the scrollable area,
  // whilst the child is attempting to be 50 pixels from the top. However, there
  // is only 25 pixels of space for the child to move into, so it should be
  // capped by that offset. As always, the child offset is relative.
  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), stickyChild->stickyPositionOffset());

  stickyParent->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());

  stickyChild->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), stickyParent->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), stickyChild->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of triple nesting. Triple (or more) nesting must be tested as the grandchild
// sticky must correct both its sticky box constraint rect and its containing
// block constaint rect.
TEST_F(LayoutBoxModelObjectTest, StickyPositionTripleNestedDiv) {
  setBodyInnerHTML(
      "<style>#scroller { height: 200px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px; }"
      "#outmostSticky { position: sticky; top: 0; height: 100px; }"
      "#middleSticky { position: sticky; top: 0; height: 75px; }"
      "#innerSticky { position: sticky; top: 25px; height: 25px; }"
      "#postPadding { height: 400px }</style>"
      "<div id='scroller'><div id='prePadding'></div><div id='outmostSticky'>"
      "<div id='middleSticky'><div id='innerSticky'></div></div></div>"
      "<div id='postPadding'></div></div>");

  LayoutBoxModelObject* outmostSticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("outmostSticky"));
  LayoutBoxModelObject* middleSticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("middleSticky"));
  LayoutBoxModelObject* innerSticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("innerSticky"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollableArea->scrollPosition().y());

  // The grandparent and parent divs are attempting to place themselves at the
  // top of the scrollable area. The child div is attempting to place itself at
  // an offset of 25 pixels to the top of the scrollable area. The result of
  // this sticky offset calculation is quite simple, but internally the child
  // offset has to offset both its sticky box constraint rect and its containing
  // block constraint rect.
  EXPECT_EQ(LayoutSize(0, 50), outmostSticky->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), middleSticky->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), innerSticky->stickyPositionOffset());

  outmostSticky->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), outmostSticky->stickyPositionOffset());

  middleSticky->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), outmostSticky->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), middleSticky->stickyPositionOffset());

  innerSticky->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 50), outmostSticky->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), middleSticky->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), innerSticky->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of tables. Tables are special as the containing block for table elements is
// always the root level <table>.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNestedStickyTable) {
  setBodyInnerHTML(
      "<style>table { border-collapse: collapse; }"
      "td, th { height: 25px; width: 25px; padding: 0; }"
      "#scroller { height: 100px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px; }"
      "#stickyDiv { position: sticky; top: 0; height: 200px; }"
      "#stickyTh { position: sticky; top: 0; }"
      "#postPadding { height: 200px; }</style>"
      "<div id='scroller'><div id='prePadding'></div><div id='stickyDiv'>"
      "<table><thead><tr><th id='stickyTh'></th></tr></thead><tbody><tr><td>"
      "</td></tr><tr><td></td></tr><tr><td></td></tr><tr><td></td></tr></tbody>"
      "</table></div><div id='postPadding'></div></div>");

  LayoutBoxModelObject* stickyDiv =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyDiv"));
  LayoutBoxModelObject* stickyTh =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stickyTh"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 150));
  ASSERT_EQ(150.0, scrollableArea->scrollPosition().y());

  // All sticky elements are attempting to stick to the top of the scrollable
  // area. For the root sticky div, this requires an offset. All the other
  // descendant sticky elements are positioned relatively so don't need offset.
  EXPECT_EQ(LayoutSize(0, 100), stickyDiv->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 0), stickyTh->stickyPositionOffset());

  // If we now scroll to the point where the overall sticky div starts to move,
  // the table headers should continue to stick to the top of the scrollable
  // area until they run out of <table> space to move in.

  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 275));
  ASSERT_EQ(275.0, scrollableArea->scrollPosition().y());

  EXPECT_EQ(LayoutSize(0, 200), stickyDiv->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), stickyTh->stickyPositionOffset());

  // Finally, if we scroll so that the table is off the top of the page, the
  // sticky header should travel as far as it can (i.e. the table height) then
  // move off the top with it.
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 350));
  ASSERT_EQ(350.0, scrollableArea->scrollPosition().y());

  EXPECT_EQ(LayoutSize(0, 200), stickyDiv->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 100), stickyTh->stickyPositionOffset());

  stickyDiv->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 200), stickyDiv->stickyPositionOffset());

  stickyTh->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 200), stickyDiv->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 100), stickyTh->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// where a particular position:sticky element is used both as a sticky-box
// shifting ancestor as well as a containing-block shifting ancestor.
//
// This is a rare case that can be replicated by nesting tables so that a sticky
// cell contains another table that has sticky elements. See the HTML below.
TEST_F(LayoutBoxModelObjectTest, StickyPositionComplexTableNesting) {
  setBodyInnerHTML(
      "<style>table { border-collapse: collapse; }"
      "td, th { height: 25px; width: 25px; padding: 0; }"
      "#scroller { height: 100px; width: 100px; overflow-y: auto; }"
      "#prePadding { height: 50px; }"
      "#outerStickyTh { height: 50px; position: sticky; top: 0; }"
      "#innerStickyTh { position: sticky; top: 25px; }"
      "#postPadding { height: 200px; }</style>"
      "<div id='scroller'><div id='prePadding'></div>"
      "<table><thead><tr><th id='outerStickyTh'><table><thead><tr>"
      "<th id='innerStickyTh'></th></tr></thead><tbody><tr><td></td></tr>"
      "</tbody></table></th></tr></thead><tbody><tr><td></td></tr><tr><td></td>"
      "</tr><tr><td></td></tr><tr><td></td></tr></tbody></table>"
      "<div id='postPadding'></div></div>");

  LayoutBoxModelObject* outerStickyTh =
      toLayoutBoxModelObject(getLayoutObjectByElementId("outerStickyTh"));
  LayoutBoxModelObject* innerStickyTh =
      toLayoutBoxModelObject(getLayoutObjectByElementId("innerStickyTh"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 150));
  ASSERT_EQ(150.0, scrollableArea->scrollPosition().y());

  EXPECT_EQ(LayoutSize(0, 100), outerStickyTh->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), innerStickyTh->stickyPositionOffset());

  outerStickyTh->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 100), outerStickyTh->stickyPositionOffset());

  innerStickyTh->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 100), outerStickyTh->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), innerStickyTh->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of nested inline elements.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNestedInlineElements) {
  setBodyInnerHTML(
      "<style>#scroller { width: 100px; height: 100px; overflow-y: scroll; }"
      "#paddingBefore { height: 50px; }"
      "#outerInline { display: inline; position: sticky; top: 0; }"
      "#innerInline { display: inline; position: sticky; top: 25px; }"
      "#paddingAfter { height: 200px; }</style>"
      "<div id='scroller'><div id='paddingBefore'></div><div id='outerInline'>"
      "<div id='innerInline'></div></div><div id='paddingAfter'></div></div>");

  LayoutBoxModelObject* outerInline =
      toLayoutBoxModelObject(getLayoutObjectByElementId("outerInline"));
  LayoutBoxModelObject* innerInline =
      toLayoutBoxModelObject(getLayoutObjectByElementId("innerInline"));

  // Scroll the page down.
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());

  EXPECT_EQ(LayoutSize(0, 0), outerInline->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), innerInline->stickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of an intermediate position:fixed element.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNestedFixedPos) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }"
      "#scroller { height: 200px; width: 100px; overflow-y: auto; }"
      "#outerSticky { position: sticky; top: 0; height: 50px; }"
      "#fixedDiv { position: fixed; top: 0; left: 300px; height: 100px; "
      "width: 100px; }"
      "#innerSticky { position: sticky; top: 25px; height: 25px; }"
      "#padding { height: 400px }</style>"
      "<div id='scroller'><div id='outerSticky'><div id='fixedDiv'>"
      "<div id='innerSticky'></div></div></div><div id='padding'></div></div>");

  LayoutBoxModelObject* outerSticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("outerSticky"));
  LayoutBoxModelObject* innerSticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("innerSticky"));

  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();

  StickyConstraintsMap constraintsMap = scrollableArea->stickyConstraintsMap();
  ASSERT_TRUE(constraintsMap.contains(outerSticky->layer()));
  ASSERT_TRUE(constraintsMap.contains(innerSticky->layer()));

  // The inner sticky should not detect the outer one as any sort of ancestor.
  EXPECT_FALSE(constraintsMap.at(innerSticky->layer())
                   .nearestStickyBoxShiftingStickyBox());
  EXPECT_FALSE(constraintsMap.at(innerSticky->layer())
                   .nearestStickyBoxShiftingContainingBlock());

  // Scroll the page down.
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollableArea->scrollPosition().y());

  // TODO(smcgruer): Until http://crbug.com/686164 is fixed, we need to update
  // the constraints here before calculations will be correct.
  innerSticky->updateStickyPositionConstraints();

  EXPECT_EQ(LayoutSize(0, 100), outerSticky->stickyPositionOffset());
  EXPECT_EQ(LayoutSize(0, 25), innerSticky->stickyPositionOffset());
}

}  // namespace blink
