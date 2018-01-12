// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositingInputsUpdater.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef bool TestParamRootLayerScrolling;
class CompositingInputsUpdaterTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  CompositingInputsUpdaterTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::Create()) {}
};

INSTANTIATE_TEST_CASE_P(All, CompositingInputsUpdaterTest, ::testing::Bool());

// Tests that transitioning a sticky away from an ancestor overflow layer that
// does not have a scrollable area does not crash.
//
// See http://crbug.com/467721#c14
TEST_P(CompositingInputsUpdaterTest,
       ChangingAncestorOverflowLayerAwayFromNonScrollableDoesNotCrash) {
  // The setup for this test is quite complex. We need UpdateRecursive to
  // transition directly from a non-scrollable ancestor overflow layer to a
  // scrollable one.
  //
  // To achieve this both scrollers must always have a PaintLayer (achieved by
  // making them positioned), and the previous ancestor overflow must change
  // from being scrollable to non-scrollable (achieved by setting its overflow
  // property to visible at the same time as we change the inner scroller.)
  SetBodyInnerHTML(R"HTML(
    <style>#outerScroller { position: relative; overflow: scroll;
    height: 500px; width: 100px; }
    #innerScroller { position: relative; height: 100px; }
    #sticky { position: sticky; top: 0; height: 50px; width: 50px; }
    #padding { height: 200px; }</style>
    <div id='outerScroller'><div id='innerScroller'><div id='sticky'></div>
    <div id='padding'></div></div></div>
  )HTML");

  LayoutBoxModelObject* outer_scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("outerScroller"));
  LayoutBoxModelObject* inner_scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("innerScroller"));
  LayoutBoxModelObject* sticky =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"));

  // Both scrollers must always have a layer.
  EXPECT_TRUE(outer_scroller->Layer());
  EXPECT_TRUE(inner_scroller->Layer());

  // The inner 'scroller' starts as non-overflow, so the sticky element's
  // ancestor overflow layer should be the outer scroller.
  EXPECT_TRUE(outer_scroller->GetScrollableArea());
  EXPECT_FALSE(inner_scroller->GetScrollableArea());
  EXPECT_TRUE(
      outer_scroller->GetScrollableArea()->GetStickyConstraintsMap().Contains(
          sticky->Layer()));
  EXPECT_EQ(sticky->Layer()->AncestorOverflowLayer(), outer_scroller->Layer());

  // Now make the outer scroller non-scrollable (i.e. overflow: visible), and
  // the inner scroller into an actual scroller.
  ToElement(outer_scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyOverflow, "visible");
  ToElement(inner_scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyOverflow, "scroll");

  // Before we update compositing inputs, validate that the current ancestor
  // overflow no longer has a scrollable area.
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_FALSE(sticky->Layer()->AncestorOverflowLayer()->GetScrollableArea());
  EXPECT_EQ(sticky->Layer()->AncestorOverflowLayer(), outer_scroller->Layer());

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Both scrollers must still have a layer.
  EXPECT_TRUE(outer_scroller->Layer());
  EXPECT_TRUE(inner_scroller->Layer());

  // However now the sticky is hanging off the inner scroller - and most
  // importantly we didnt crash when we switched ancestor overflow layers.
  EXPECT_TRUE(inner_scroller->GetScrollableArea());
  EXPECT_TRUE(
      inner_scroller->GetScrollableArea()->GetStickyConstraintsMap().Contains(
          sticky->Layer()));
  EXPECT_EQ(sticky->Layer()->AncestorOverflowLayer(), inner_scroller->Layer());
}

TEST_P(CompositingInputsUpdaterTest, UnclippedAndClippedRectsUnderScroll) {
  SetBodyInnerHTML(R"HTML(
    <div id=clip style="overflow: hidden; position: relative">
      <div id=target style="transform: translateZ(0); width: 200px; height: 200px; background: lightgray"></div>
     </div>
     <div style="position: relative; width: 20px; height: 3000px"></div>
  )HTML");

  LayoutBoxModelObject* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"));

  GetDocument().View()->LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(0, 25), kUserScroll);
  GetDocument()
      .View()
      ->GetLayoutView()
      ->Layer()
      ->SetNeedsCompositingInputsUpdate();
  GetDocument().View()->UpdateAllLifecyclePhases();
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_EQ(IntRect(8, -17, 200, 200),
              target->Layer()->ClippedAbsoluteBoundingBox());
    EXPECT_EQ(IntRect(8, -17, 200, 200),
              target->Layer()->UnclippedAbsoluteBoundingBox());
  } else {
    EXPECT_EQ(IntRect(8, 8, 200, 200),
              target->Layer()->ClippedAbsoluteBoundingBox());
    EXPECT_EQ(IntRect(8, 8, 200, 200),
              target->Layer()->UnclippedAbsoluteBoundingBox());
  }
}

}  // namespace blink
