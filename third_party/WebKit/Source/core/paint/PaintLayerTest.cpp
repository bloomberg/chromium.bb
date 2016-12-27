// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayer.h"

#include "core/html/HTMLIFrameElement.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"

namespace blink {

typedef std::pair<bool, bool> SlimmingPaintAndRootLayerScrolling;
class PaintLayerTest
    : public ::testing::WithParamInterface<SlimmingPaintAndRootLayerScrolling>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  PaintLayerTest()
      : ScopedSlimmingPaintV2ForTest(GetParam().first),
        ScopedRootLayerScrollingForTest(GetParam().second),
        RenderingTest(SingleChildFrameLoaderClient::create()) {}
};

SlimmingPaintAndRootLayerScrolling foo[] = {
    SlimmingPaintAndRootLayerScrolling(false, false),
    SlimmingPaintAndRootLayerScrolling(true, false),
    SlimmingPaintAndRootLayerScrolling(false, true),
    SlimmingPaintAndRootLayerScrolling(true, true)};

INSTANTIATE_TEST_CASE_P(All, PaintLayerTest, ::testing::ValuesIn(foo));

TEST_P(PaintLayerTest, CompositedBoundsAbsPosGrandchild) {
  // TODO(chrishtr): fix this test for SPv2
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      " <div id='parent'><div id='absposparent'><div id='absposchild'>"
      " </div></div></div>"
      "<style>"
      "  #parent { position: absolute; z-index: 0; overflow: hidden;"
      "  background: lightgray; width: 150px; height: 150px;"
      "  will-change: transform; }"
      "  #absposparent { position: absolute; z-index: 0; }"
      "  #absposchild { position: absolute; top: 0px; left: 0px; height: 200px;"
      "  width: 200px; background: lightblue; }</style>");

  PaintLayer* parentLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  // Since "absposchild" is clipped by "parent", it should not expand the
  // composited bounds for "parent" beyond its intrinsic size of 150x150.
  EXPECT_EQ(LayoutRect(0, 0, 150, 150),
            parentLayer->boundingBoxForCompositing());
}

TEST_P(PaintLayerTest, PaintingExtentReflection) {
  setBodyInnerHTML(
      "<div id='target' style='background-color: blue; position: absolute;"
      "    width: 110px; height: 120px; top: 40px; left: 60px;"
      "    -webkit-box-reflect: below 3px'>"
      "</div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_EQ(
      LayoutRect(60, 40, 110, 243),
      layer->paintingExtent(document().layoutView()->layer(), LayoutSize(), 0));
}

TEST_P(PaintLayerTest, PaintingExtentReflectionWithTransform) {
  setBodyInnerHTML(
      "<div id='target' style='background-color: blue; position: absolute;"
      "    width: 110px; height: 120px; top: 40px; left: 60px;"
      "    -webkit-box-reflect: below 3px; transform: translateX(30px)'>"
      "</div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_EQ(
      LayoutRect(90, 40, 110, 243),
      layer->paintingExtent(document().layoutView()->layer(), LayoutSize(), 0));
}

TEST_P(PaintLayerTest, ScrollsWithViewportRelativePosition) {
  setBodyInnerHTML("<div id='target' style='position: relative'></div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_FALSE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportFixedPosition) {
  setBodyInnerHTML("<div id='target' style='position: fixed'></div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_TRUE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportFixedPositionInsideTransform) {
  // We don't intend to launch SPv2 without root layer scrolling, so skip this
  // test in that configuration because it's broken.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      !RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return;
  setBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: fixed'></div>"
      "</div>"
      "<div style='width: 10px; height: 1000px'></div>");
  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_FALSE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest,
       ScrollsWithViewportFixedPositionInsideTransformNoScroll) {
  setBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: fixed'></div>"
      "</div>");
  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();

  // In SPv2 mode, we correctly determine that the frame doesn't scroll at all,
  // and so return true.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    EXPECT_TRUE(layer->sticksToViewport());
  else
    EXPECT_FALSE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportStickyPosition) {
  setBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: sticky'></div>"
      "</div>"
      "<div style='width: 10px; height: 1000px'></div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_TRUE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportStickyPositionNoScroll) {
  setBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: sticky'></div>"
      "</div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_TRUE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportStickyPositionInsideScroller) {
  setBodyInnerHTML(
      "<div style='overflow:scroll; width: 100px; height: 100px;'>"
      "  <div id='target' style='position: sticky'></div>"
      "  <div style='width: 50px; height: 1000px;'></div>"
      "</div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_FALSE(layer->sticksToViewport());
}

TEST_P(PaintLayerTest, CompositedScrollingNoNeedsRepaint) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  enableCompositing();
  setBodyInnerHTML(
      "<div id='scroll' style='width: 100px; height: 100px; overflow: scroll;"
      "    will-change: transform'>"
      "  <div id='content' style='position: relative; background: blue;"
      "      width: 2000px; height: 2000px'></div>"
      "</div>");

  PaintLayer* scrollLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroll"))->layer();
  EXPECT_EQ(PaintsIntoOwnBacking, scrollLayer->compositingState());

  PaintLayer* contentLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("content"))->layer();
  EXPECT_EQ(NotComposited, contentLayer->compositingState());
  EXPECT_EQ(LayoutPoint(), contentLayer->location());

  scrollLayer->getScrollableArea()->setScrollOffset(ScrollOffset(1000, 1000),
                                                    ProgrammaticScroll);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(LayoutPoint(-1000, -1000), contentLayer->location());
  EXPECT_FALSE(contentLayer->needsRepaint());
  EXPECT_FALSE(scrollLayer->needsRepaint());
  document().view()->updateAllLifecyclePhases();
}

TEST_P(PaintLayerTest, NonCompositedScrollingNeedsRepaint) {
  setBodyInnerHTML(
      "<div id='scroll' style='width: 100px; height: 100px; overflow: scroll'>"
      "  <div id='content' style='position: relative; background: blue;"
      "      width: 2000px; height: 2000px'></div>"
      "</div>");

  PaintLayer* scrollLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroll"))->layer();
  EXPECT_EQ(NotComposited, scrollLayer->compositingState());

  PaintLayer* contentLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("content"))->layer();
  EXPECT_EQ(NotComposited, scrollLayer->compositingState());
  EXPECT_EQ(LayoutPoint(), contentLayer->location());

  scrollLayer->getScrollableArea()->setScrollOffset(ScrollOffset(1000, 1000),
                                                    ProgrammaticScroll);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(LayoutPoint(-1000, -1000), contentLayer->location());
  EXPECT_TRUE(contentLayer->needsRepaint());
  EXPECT_TRUE(scrollLayer->needsRepaint());
  document().view()->updateAllLifecyclePhases();
}

TEST_P(PaintLayerTest, HasNonIsolatedDescendantWithBlendMode) {
  setBodyInnerHTML(
      "<div id='stacking-grandparent' style='isolation: isolate'>"
      "  <div id='stacking-parent' style='isolation: isolate'>"
      "    <div id='non-stacking-parent' style='position:relative'>"
      "      <div id='blend-mode' style='mix-blend-mode: overlay'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</div>");
  PaintLayer* stackingGrandparent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stacking-grandparent"))
          ->layer();
  PaintLayer* stackingParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("stacking-parent"))
          ->layer();
  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("non-stacking-parent"))
          ->layer();

  EXPECT_TRUE(parent->hasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(stackingParent->hasNonIsolatedDescendantWithBlendMode());
  EXPECT_FALSE(stackingGrandparent->hasNonIsolatedDescendantWithBlendMode());

  EXPECT_FALSE(parent->hasDescendantWithClipPath());
  EXPECT_TRUE(parent->hasVisibleDescendant());
}

TEST_P(PaintLayerTest, HasDescendantWithClipPath) {
  setBodyInnerHTML(
      "<div id='parent' style='position:relative'>"
      "  <div id='clip-path' style='clip-path: circle(50px at 0 100px)'>"
      "  </div>"
      "</div>");
  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* clipPath =
      toLayoutBoxModelObject(getLayoutObjectByElementId("clip-path"))->layer();

  EXPECT_TRUE(parent->hasDescendantWithClipPath());
  EXPECT_FALSE(clipPath->hasDescendantWithClipPath());

  EXPECT_FALSE(parent->hasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(parent->hasVisibleDescendant());
}

TEST_P(PaintLayerTest, HasVisibleDescendant) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='invisible' style='position:relative'>"
      "  <div id='visible' style='visibility: visible; position: relative'>"
      "  </div>"
      "</div>");
  PaintLayer* invisible =
      toLayoutBoxModelObject(getLayoutObjectByElementId("invisible"))->layer();
  PaintLayer* visible =
      toLayoutBoxModelObject(getLayoutObjectByElementId("visible"))->layer();

  EXPECT_TRUE(invisible->hasVisibleDescendant());
  EXPECT_FALSE(visible->hasVisibleDescendant());

  EXPECT_FALSE(invisible->hasNonIsolatedDescendantWithBlendMode());
  EXPECT_FALSE(invisible->hasDescendantWithClipPath());
}

TEST_P(PaintLayerTest, DescendantDependentFlagsStopsAtThrottledFrames) {
  enableCompositing();
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='transform' style='transform: translate3d(4px, 5px, 6px);'>"
      "</div>"
      "<iframe id='iframe' sandbox></iframe>");
  setChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='iframeTransform'"
      "  style='transform: translate3d(4px, 5px, 6px);'/>");

  // Move the child frame offscreen so it becomes available for throttling.
  auto* iframe = toHTMLIFrameElement(document().getElementById("iframe"));
  iframe->setAttribute(HTMLNames::styleAttr, "transform: translateY(5555px)");
  document().view()->updateAllLifecyclePhases();
  // Ensure intersection observer notifications get delivered.
  testing::runPendingTasks();
  EXPECT_FALSE(document().view()->isHiddenForThrottling());
  EXPECT_TRUE(childDocument().view()->isHiddenForThrottling());

  {
    DocumentLifecycle::AllowThrottlingScope throttlingScope(
        document().lifecycle());
    EXPECT_FALSE(document().view()->shouldThrottleRendering());
    EXPECT_TRUE(childDocument().view()->shouldThrottleRendering());

    childDocument().view()->layoutView()->layer()->dirtyVisibleContentStatus();

    EXPECT_TRUE(childDocument()
                    .view()
                    ->layoutView()
                    ->layer()
                    ->m_needsDescendantDependentFlagsUpdate);

    // Also check that the rest of the lifecycle succeeds without crashing due
    // to a stale m_needsDescendantDependentFlagsUpdate.
    document().view()->updateAllLifecyclePhases();

    // Still dirty, because the frame was throttled.
    EXPECT_TRUE(childDocument()
                    .view()
                    ->layoutView()
                    ->layer()
                    ->m_needsDescendantDependentFlagsUpdate);
  }

  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(childDocument()
                   .view()
                   ->layoutView()
                   ->layer()
                   ->m_needsDescendantDependentFlagsUpdate);
}

TEST_P(PaintLayerTest, PaintInvalidationOnNonCompositedScroll) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      "<style>* { margin: 0 } ::-webkit-scrollbar { display: none }</style>"
      "<div id='scroller' style='overflow: scroll; width: 50px; height: 50px'>"
      "  <div style='height: 400px'>"
      "    <div id='content-layer' style='position: relative; height: 10px;"
      "        top: 30px; background: blue'>"
      "      <div id='content' style='height: 5px; background: yellow'></div>"
      "    </div>"
      "  </div>"
      "</div>");

  LayoutBox* scroller = toLayoutBox(getLayoutObjectByElementId("scroller"));
  LayoutObject* contentLayer = getLayoutObjectByElementId("content-layer");
  LayoutObject* content = getLayoutObjectByElementId("content");
  EXPECT_EQ(LayoutRect(0, 30, 50, 10), contentLayer->visualRect());
  EXPECT_EQ(LayoutRect(0, 30, 50, 5), content->visualRect());

  scroller->getScrollableArea()->setScrollOffset(ScrollOffset(0, 20),
                                                 ProgrammaticScroll);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 10, 50, 10), contentLayer->visualRect());
  EXPECT_EQ(LayoutRect(0, 10, 50, 5), content->visualRect());
}

TEST_P(PaintLayerTest, PaintInvalidationOnCompositedScroll) {
  enableCompositing();
  setBodyInnerHTML(
      "<style>* { margin: 0 } ::-webkit-scrollbar { display: none }</style>"
      "<div id='scroller' style='overflow: scroll; width: 50px; height: 50px;"
      "    will-change: transform'>"
      "  <div style='height: 400px'>"
      "    <div id='content-layer' style='position: relative; height: 10px;"
      "        top: 30px; background: blue'>"
      "      <div id='content' style='height: 5px; background: yellow'></div>"
      "    </div>"
      "  </div>"
      "</div>");

  LayoutBox* scroller = toLayoutBox(getLayoutObjectByElementId("scroller"));
  LayoutObject* contentLayer = getLayoutObjectByElementId("content-layer");
  LayoutObject* content = getLayoutObjectByElementId("content");
  EXPECT_EQ(LayoutRect(0, 30, 50, 10), contentLayer->visualRect());
  EXPECT_EQ(LayoutRect(0, 30, 50, 5), content->visualRect());

  scroller->getScrollableArea()->setScrollOffset(ScrollOffset(0, 20),
                                                 ProgrammaticScroll);
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 30, 50, 10), contentLayer->visualRect());
  EXPECT_EQ(LayoutRect(0, 30, 50, 5), content->visualRect());
}

}  // namespace blink
