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
        RenderingTest(SingleChildLocalFrameClient::create()) {}
};

SlimmingPaintAndRootLayerScrolling foo[] = {
    SlimmingPaintAndRootLayerScrolling(false, false),
    SlimmingPaintAndRootLayerScrolling(true, false),
    SlimmingPaintAndRootLayerScrolling(false, true),
    SlimmingPaintAndRootLayerScrolling(true, true)};

INSTANTIATE_TEST_CASE_P(All, PaintLayerTest, ::testing::ValuesIn(foo));

TEST_P(PaintLayerTest, ChildWithoutPaintLayer) {
  setBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px;'></div>");

  Element* element = document().getElementById("target");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(element->layoutObject())->layer();
  PaintLayer* rootLayer = layoutView().layer();

  EXPECT_EQ(nullptr, paintLayer);
  EXPECT_NE(nullptr, rootLayer);
}

TEST_P(PaintLayerTest, CompositedBoundsAbsPosGrandchild) {
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

TEST_P(PaintLayerTest, CompositedBoundsTransformedChild) {
  // TODO(chrishtr): fix this test for SPv2
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      "<div id=parent style='overflow: scroll; will-change: transform'>"
      "  <div class='target'"
      "       style='position: relative; transform: skew(-15deg);'>"
      "  </div>"
      "  <div style='width: 1000px; height: 500px; background: lightgray'>"
      "  </div>"
      "</div>");

  PaintLayer* parentLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  EXPECT_EQ(LayoutRect(0, 0, 784, 500),
            parentLayer->boundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerCompositedBounds) {
  setBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  EXPECT_EQ(RuntimeEnabledFeatures::rootLayerScrollingEnabled()
                ? LayoutRect(0, 0, 800, 600)
                : LayoutRect(0, 0, 1000, 1000),
            layoutView().layer()->boundingBoxForCompositing());
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

TEST_P(PaintLayerTest, Has3DTransformedDescendant) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='parent' style='position:relative; z-index: 0'>"
      "  <div id='child' style='transform: translateZ(1px)'>"
      "  </div>"
      "</div>");
  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();

  EXPECT_TRUE(parent->has3DTransformedDescendant());
  EXPECT_FALSE(child->has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantChangeStyle) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='parent' style='position:relative; z-index: 0'>"
      "  <div id='child' style='position:relative '>"
      "  </div>"
      "</div>");
  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();

  EXPECT_FALSE(parent->has3DTransformedDescendant());
  EXPECT_FALSE(child->has3DTransformedDescendant());

  document().getElementById("child")->setAttribute(
      HTMLNames::styleAttr, "transform: translateZ(1px)");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(parent->has3DTransformedDescendant());
  EXPECT_FALSE(child->has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantNotStacking) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='parent' style='position:relative;'>"
      "  <div id='child' style='transform: translateZ(1px)'>"
      "  </div>"
      "</div>");
  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();

  // |child| is not a stacking child of |parent|, so it has no 3D transformed
  // descendant.
  EXPECT_FALSE(parent->has3DTransformedDescendant());
  EXPECT_FALSE(child->has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedGrandchildWithPreserve3d) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='parent' style='position:relative; z-index: 0'>"
      "  <div id='child' style='transform-style: preserve-3d'>"
      "    <div id='grandchild' style='transform: translateZ(1px)'>"
      "    </div>"
      "  </div>"
      "</div>");
  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();
  PaintLayer* grandchild =
      toLayoutBoxModelObject(getLayoutObjectByElementId("grandchild"))->layer();

  EXPECT_TRUE(parent->has3DTransformedDescendant());
  EXPECT_TRUE(child->has3DTransformedDescendant());
  EXPECT_FALSE(grandchild->has3DTransformedDescendant());
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

TEST_P(PaintLayerTest, CompositingContainerFloat) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <div style='backface-visibility: hidden'></div>"
      "    <span id='span'"
      "        style='clip-path: polygon(0px 15px, 0px 54px, 100px 0px)'>"
      "      <div id='target' style='float: right; position: relative'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();
  EXPECT_EQ(span, target->compositingContainer());
  PaintLayer* compositedContainer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("compositedContainer"))
          ->layer();

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(compositedContainer,
              target->enclosingLayerWithCompositedLayerMapping(ExcludeSelf));
  }
}

TEST_P(PaintLayerTest, FloatLayerAndAbsoluteUnderInlineLayer) {
  setBodyInnerHTML(
      "<div id='container' style='position: absolute; top: 20px; left: 20px'>"
      "  <div style='margin: 33px'>"
      "    <span id='span' style='position: relative; top: 100px; left: 100px'>"
      "      <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "      </div>"
      "      <div id='absolute'"
      "        style='position: absolute; top: 50px; left: 50px'>"
      "      </div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* floating =
      toLayoutBoxModelObject(getLayoutObjectByElementId("floating"))->layer();
  PaintLayer* absolute =
      toLayoutBoxModelObject(getLayoutObjectByElementId("absolute"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();
  PaintLayer* container =
      toLayoutBoxModelObject(getLayoutObjectByElementId("container"))->layer();

  EXPECT_EQ(span, floating->parent());
  EXPECT_EQ(container, floating->containingLayer());
  EXPECT_EQ(span, absolute->parent());
  EXPECT_EQ(span, absolute->containingLayer());
  EXPECT_EQ(container, span->parent());
  EXPECT_EQ(container, span->containingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), floating->location());
  EXPECT_EQ(LayoutPoint(50, 50), absolute->location());
  EXPECT_EQ(LayoutPoint(133, 133), span->location());
  EXPECT_EQ(LayoutPoint(20, 20), container->location());

  EXPECT_EQ(LayoutPoint(-50, -50), floating->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(50, 50), absolute->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), floating->visualOffsetFromAncestor(container));
  EXPECT_EQ(LayoutPoint(183, 183),
            absolute->visualOffsetFromAncestor(container));
}

TEST_P(PaintLayerTest, FloatLayerUnderInlineLayerScrolled) {
  setBodyInnerHTML(
      "<div id='container' style='overflow: scroll; width: 50px; height: 50px'>"
      "  <span id='span' style='position: relative; top: 100px; left: 100px'>"
      "    <div id='floating'"
      "      style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </span>"
      "  <div style='height: 1000px'></div>"
      "</div>");

  PaintLayer* floating =
      toLayoutBoxModelObject(getLayoutObjectByElementId("floating"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();
  PaintLayer* container =
      toLayoutBoxModelObject(getLayoutObjectByElementId("container"))->layer();
  container->getScrollableArea()->setScrollOffset(ScrollOffset(0, 400),
                                                  ProgrammaticScroll);

  EXPECT_EQ(span, floating->parent());
  EXPECT_EQ(container, floating->containingLayer());
  EXPECT_EQ(container, span->parent());
  EXPECT_EQ(container, span->containingLayer());

  EXPECT_EQ(LayoutPoint(50, -350), floating->location());
  EXPECT_EQ(LayoutPoint(100, -300), span->location());

  EXPECT_EQ(LayoutPoint(-50, -50), floating->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(50, -350),
            floating->visualOffsetFromAncestor(container));
}

TEST_P(PaintLayerTest, FloatLayerUnderBlockUnderInlineLayer) {
  setBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div style='display: inline-block; margin: 33px'>"
      "    <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* floating =
      toLayoutBoxModelObject(getLayoutObjectByElementId("floating"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();

  EXPECT_EQ(span, floating->parent());
  EXPECT_EQ(span, floating->containingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), floating->location());
  EXPECT_EQ(LayoutPoint(100, 100), span->location());
  EXPECT_EQ(LayoutPoint(83, 83), floating->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(183, 183), floating->visualOffsetFromAncestor(
                                       document().layoutView()->layer()));
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatUnderInlineLayer) {
  setBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div style='float: left; margin: 33px'>"
      "    <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* floating =
      toLayoutBoxModelObject(getLayoutObjectByElementId("floating"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();

  EXPECT_EQ(span, floating->parent());
  EXPECT_EQ(span->parent(), floating->containingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), floating->location());
  EXPECT_EQ(LayoutPoint(100, 100), span->location());
  EXPECT_EQ(LayoutPoint(-17, -17), floating->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), floating->visualOffsetFromAncestor(
                                     document().layoutView()->layer()));
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatLayerUnderInlineLayer) {
  setBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div id='floatingParent'"
      "      style='float: left; position: relative; margin: 33px'>"
      "    <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* floating =
      toLayoutBoxModelObject(getLayoutObjectByElementId("floating"))->layer();
  PaintLayer* floatingParent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("floatingParent"))
          ->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();

  EXPECT_EQ(floatingParent, floating->parent());
  EXPECT_EQ(floatingParent, floating->containingLayer());
  EXPECT_EQ(span, floatingParent->parent());
  EXPECT_EQ(span->parent(), floatingParent->containingLayer());

  EXPECT_EQ(LayoutPoint(50, 50), floating->location());
  EXPECT_EQ(LayoutPoint(33, 33), floatingParent->location());
  EXPECT_EQ(LayoutPoint(100, 100), span->location());
  EXPECT_EQ(LayoutPoint(-17, -17), floating->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(-67, -67),
            floatingParent->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), floating->visualOffsetFromAncestor(
                                     document().layoutView()->layer()));
}

TEST_P(PaintLayerTest, LayerUnderFloatUnderInlineLayer) {
  setBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div style='float: left; margin: 33px'>"
      "    <div>"
      "      <div id='child' style='position: relative; top: 50px; left: 50px'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();

  EXPECT_EQ(span, child->parent());
  EXPECT_EQ(span->parent(), child->containingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), child->location());
  EXPECT_EQ(LayoutPoint(100, 100), span->location());
  EXPECT_EQ(LayoutPoint(-17, -17), child->visualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83),
            child->visualOffsetFromAncestor(document().layoutView()->layer()));
}

TEST_P(PaintLayerTest, CompositingContainerFloatingIframe) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <div style='backface-visibility: hidden'></div>"
      "    <span id='span'"
      "        style='clip-path: polygon(0px 15px, 0px 54px, 100px 0px)'>"
      "      <iframe srcdoc='foo' id='target' style='float: right'></iframe>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();

  // A non-positioned iframe still gets a PaintLayer because PaintLayers are
  // forced for all LayoutPart objects. However, such PaintLayers are not
  // stacked.
  PaintLayer* containingBlock =
      toLayoutBoxModelObject(getLayoutObjectByElementId("containingBlock"))
          ->layer();
  EXPECT_EQ(containingBlock, target->compositingContainer());
  PaintLayer* compositedContainer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("compositedContainer"))
          ->layer();

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(compositedContainer,
              target->enclosingLayerWithCompositedLayerMapping(ExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerSelfPaintingNonStackedFloat) {
  setBodyInnerHTML(
      "<div id='container' style='position: relative'>"
      "  <span id='span' style='opacity: 0.9'>"
      "    <div id='target' style='columns: 1; float: left'></div>"
      "  </span>"
      "</div>");

  // The target layer is self-painting, but not stacked.
  PaintLayer* target =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_TRUE(target->isSelfPaintingLayer());
  EXPECT_FALSE(target->stackingNode()->isStacked());

  PaintLayer* container =
      toLayoutBoxModelObject(getLayoutObjectByElementId("container"))->layer();
  PaintLayer* span =
      toLayoutBoxModelObject(getLayoutObjectByElementId("span"))->layer();
  EXPECT_EQ(container, target->containingLayer());
  EXPECT_EQ(span, target->compositingContainer());
}

TEST_P(PaintLayerTest, ColumnSpanLayerUnderExtraLayerScrolled) {
  setBodyInnerHTML(
      "<div id='columns' style='overflow: hidden; width: 80px; height: 80px; "
      "    columns: 2; column-gap: 0'>"
      "  <div id='extraLayer'"
      "      style='position: relative; top: 100px; left: 100px'>"
      "    <div id='spanner' style='column-span: all; position: relative; "
      "        top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "  <div style='height: 1000px'></div>"
      "</div>");

  PaintLayer* spanner =
      toLayoutBoxModelObject(getLayoutObjectByElementId("spanner"))->layer();
  PaintLayer* extraLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("extraLayer"))->layer();
  PaintLayer* columns =
      toLayoutBoxModelObject(getLayoutObjectByElementId("columns"))->layer();
  columns->getScrollableArea()->setScrollOffset(ScrollOffset(200, 0),
                                                ProgrammaticScroll);

  EXPECT_EQ(extraLayer, spanner->parent());
  EXPECT_EQ(columns, spanner->containingLayer());
  EXPECT_EQ(columns, extraLayer->parent()->parent());
  EXPECT_EQ(columns, extraLayer->containingLayer()->parent());

  EXPECT_EQ(LayoutPoint(-150, 50), spanner->location());
  EXPECT_EQ(LayoutPoint(100, 100), extraLayer->location());
  // -60 = 2nd-column-x(40) - scroll-offset-x(200) + x-location(100)
  // 20 = y-location(100) - column-height(80)
  EXPECT_EQ(LayoutPoint(-60, 20),
            extraLayer->visualOffsetFromAncestor(columns));
  EXPECT_EQ(LayoutPoint(-150, 50), spanner->visualOffsetFromAncestor(columns));
}

}  // namespace blink
