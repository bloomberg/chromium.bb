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
        RenderingTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  PaintLayer* GetPaintLayerByElementId(const char* id) {
    return ToLayoutBoxModelObject(GetLayoutObjectByElementId(id))->Layer();
  }
};

SlimmingPaintAndRootLayerScrolling g_foo[] = {
    SlimmingPaintAndRootLayerScrolling(false, false),
    SlimmingPaintAndRootLayerScrolling(true, false),
    SlimmingPaintAndRootLayerScrolling(false, true),
    SlimmingPaintAndRootLayerScrolling(true, true)};

INSTANTIATE_TEST_CASE_P(All, PaintLayerTest, ::testing::ValuesIn(g_foo));

TEST_P(PaintLayerTest, ChildWithoutPaintLayer) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px;'></div>");

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  PaintLayer* root_layer = GetLayoutView().Layer();

  EXPECT_EQ(nullptr, paint_layer);
  EXPECT_NE(nullptr, root_layer);
}

TEST_P(PaintLayerTest, CompositedBoundsAbsPosGrandchild) {
  SetBodyInnerHTML(
      " <div id='parent'><div id='absposparent'><div id='absposchild'>"
      " </div></div></div>"
      "<style>"
      "  #parent { position: absolute; z-index: 0; overflow: hidden;"
      "  background: lightgray; width: 150px; height: 150px;"
      "  will-change: transform; }"
      "  #absposparent { position: absolute; z-index: 0; }"
      "  #absposchild { position: absolute; top: 0px; left: 0px; height: 200px;"
      "  width: 200px; background: lightblue; }</style>");

  PaintLayer* parent_layer = GetPaintLayerByElementId("parent");
  // Since "absposchild" is clipped by "parent", it should not expand the
  // composited bounds for "parent" beyond its intrinsic size of 150x150.
  EXPECT_EQ(LayoutRect(0, 0, 150, 150),
            parent_layer->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, CompositedBoundsTransformedChild) {
  // TODO(chrishtr): fix this test for SPv2
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(
      "<div id=parent style='overflow: scroll; will-change: transform'>"
      "  <div class='target'"
      "       style='position: relative; transform: skew(-15deg);'>"
      "  </div>"
      "  <div style='width: 1000px; height: 500px; background: lightgray'>"
      "  </div>"
      "</div>");

  PaintLayer* parent_layer = GetPaintLayerByElementId("parent");
  EXPECT_EQ(LayoutRect(0, 0, 784, 500),
            parent_layer->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerCompositedBounds) {
  SetBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  EXPECT_EQ(RuntimeEnabledFeatures::RootLayerScrollingEnabled()
                ? LayoutRect(0, 0, 800, 600)
                : LayoutRect(0, 0, 1000, 1000),
            GetLayoutView().Layer()->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerScrollBounds) {
  if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    return;
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);

  SetBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  PaintLayerScrollableArea* plsa = GetLayoutView().Layer()->GetScrollableArea();

  int scrollbarThickness = plsa->VerticalScrollbarWidth();
  EXPECT_EQ(scrollbarThickness, plsa->HorizontalScrollbarHeight());
  EXPECT_GT(scrollbarThickness, 0);

  EXPECT_EQ(ScrollOffset(200 + scrollbarThickness, 400 + scrollbarThickness),
            plsa->MaximumScrollOffset());

  EXPECT_EQ(IntRect(0, 0, 800 - scrollbarThickness, 600 - scrollbarThickness),
            plsa->VisibleContentRect());
  EXPECT_EQ(IntRect(0, 0, 800, 600),
            plsa->VisibleContentRect(kIncludeScrollbars));
}

TEST_P(PaintLayerTest, PaintingExtentReflection) {
  SetBodyInnerHTML(
      "<div id='target' style='background-color: blue; position: absolute;"
      "    width: 110px; height: 120px; top: 40px; left: 60px;"
      "    -webkit-box-reflect: below 3px'>"
      "</div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(LayoutRect(60, 40, 110, 243),
            layer->PaintingExtent(GetDocument().GetLayoutView()->Layer(),
                                  LayoutSize(), 0));
}

TEST_P(PaintLayerTest, PaintingExtentReflectionWithTransform) {
  SetBodyInnerHTML(
      "<div id='target' style='background-color: blue; position: absolute;"
      "    width: 110px; height: 120px; top: 40px; left: 60px;"
      "    -webkit-box-reflect: below 3px; transform: translateX(30px)'>"
      "</div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(LayoutRect(90, 40, 110, 243),
            layer->PaintingExtent(GetDocument().GetLayoutView()->Layer(),
                                  LayoutSize(), 0));
}

TEST_P(PaintLayerTest, ScrollsWithViewportRelativePosition) {
  SetBodyInnerHTML("<div id='target' style='position: relative'></div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_FALSE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportFixedPosition) {
  SetBodyInnerHTML("<div id='target' style='position: fixed'></div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportFixedPositionInsideTransform) {
  // We don't intend to launch SPv2 without root layer scrolling, so skip this
  // test in that configuration because it's broken.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      !RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    return;
  SetBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: fixed'></div>"
      "</div>"
      "<div style='width: 10px; height: 1000px'></div>");
  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_FALSE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest,
       ScrollsWithViewportFixedPositionInsideTransformNoScroll) {
  SetBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: fixed'></div>"
      "</div>");
  PaintLayer* layer = GetPaintLayerByElementId("target");

  // In SPv2 mode, we correctly determine that the frame doesn't scroll at all,
  // and so return true.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_TRUE(layer->FixedToViewport());
  else
    EXPECT_FALSE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPosition) {
  SetBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: sticky; top: 0;'></div>"
      "</div>"
      "<div style='width: 10px; height: 1000px'></div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerNoAnchor) {
  SetBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: sticky'></div>"
      "</div>"
      "<div style='width: 10px; height: 1000px'></div>");

  PaintLayer* layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  EXPECT_FALSE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPositionNoScroll) {
  SetBodyInnerHTML(
      "<div style='transform: translateZ(0)'>"
      "  <div id='target' style='position: sticky; top: 0;'></div>"
      "</div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPositionInsideScroller) {
  SetBodyInnerHTML(
      "<div style='overflow:scroll; width: 100px; height: 100px;'>"
      "  <div id='target' style='position: sticky; top: 0;'></div>"
      "  <div style='width: 50px; height: 1000px;'></div>"
      "</div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, CompositedScrollingNoNeedsRepaint) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='scroll' style='width: 100px; height: 100px; overflow: scroll;"
      "    will-change: transform'>"
      "  <div id='content' style='position: relative; background: blue;"
      "      width: 2000px; height: 2000px'></div>"
      "</div>");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");
  EXPECT_EQ(kPaintsIntoOwnBacking, scroll_layer->GetCompositingState());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  EXPECT_EQ(kNotComposited, content_layer->GetCompositingState());
  EXPECT_EQ(LayoutPoint(), content_layer->Location());

  scroll_layer->GetScrollableArea()->SetScrollOffset(ScrollOffset(1000, 1000),
                                                     kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(LayoutPoint(-1000, -1000), content_layer->Location());
  EXPECT_FALSE(content_layer->NeedsRepaint());
  EXPECT_FALSE(scroll_layer->NeedsRepaint());
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_P(PaintLayerTest, NonCompositedScrollingNeedsRepaint) {
  SetBodyInnerHTML(
      "<div id='scroll' style='width: 100px; height: 100px; overflow: scroll'>"
      "  <div id='content' style='position: relative; background: blue;"
      "      width: 2000px; height: 2000px'></div>"
      "</div>");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");
  EXPECT_EQ(kNotComposited, scroll_layer->GetCompositingState());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  EXPECT_EQ(kNotComposited, scroll_layer->GetCompositingState());
  EXPECT_EQ(LayoutPoint(), content_layer->Location());

  scroll_layer->GetScrollableArea()->SetScrollOffset(ScrollOffset(1000, 1000),
                                                     kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(LayoutPoint(-1000, -1000), content_layer->Location());
  EXPECT_TRUE(content_layer->NeedsRepaint());
  EXPECT_TRUE(scroll_layer->NeedsRepaint());
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_P(PaintLayerTest, HasNonIsolatedDescendantWithBlendMode) {
  SetBodyInnerHTML(
      "<div id='stacking-grandparent' style='isolation: isolate'>"
      "  <div id='stacking-parent' style='isolation: isolate'>"
      "    <div id='non-stacking-parent' style='position:relative'>"
      "      <div id='blend-mode' style='mix-blend-mode: overlay'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</div>");
  PaintLayer* stacking_grandparent =
      GetPaintLayerByElementId("stacking-grandparent");
  PaintLayer* stacking_parent = GetPaintLayerByElementId("stacking-parent");
  PaintLayer* parent = GetPaintLayerByElementId("non-stacking-parent");

  EXPECT_TRUE(parent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(stacking_parent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_FALSE(stacking_grandparent->HasNonIsolatedDescendantWithBlendMode());

  EXPECT_FALSE(parent->HasDescendantWithClipPath());
  EXPECT_TRUE(parent->HasVisibleDescendant());
}

TEST_P(PaintLayerTest, SubsequenceCachingStackingContexts) {
  SetBodyInnerHTML(
      "<div id='parent' style='position:relative'>"
      "  <div id='child1' style='position: relative'>"
      "    <div id='grandchild1' style='position: relative'>"
      "      <div style='position: relative'></div>"
      "    </div>"
      "  </div>"
      "  <div id='child2' style='isolation: isolate'>"
      "    <div style='position: relative'></div>"
      "  </div>"
      "</div>");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child1 = GetPaintLayerByElementId("child1");
  PaintLayer* child2 = GetPaintLayerByElementId("child2");
  PaintLayer* grandchild1 = GetPaintLayerByElementId("grandchild1");

  EXPECT_FALSE(parent->SupportsSubsequenceCaching());
  EXPECT_FALSE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild1->SupportsSubsequenceCaching());

  GetDocument()
      .getElementById("grandchild1")
      ->setAttribute(HTMLNames::styleAttr, "isolation: isolate");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(parent->SupportsSubsequenceCaching());
  EXPECT_FALSE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_TRUE(grandchild1->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingSVGRoot) {
  SetBodyInnerHTML(
      "<div id='parent' style='position: relative'>"
      "  <svg id='svgroot' style='position: relative'></svg>"
      "</div>");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  EXPECT_FALSE(svgroot->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingMuticol) {
  SetBodyInnerHTML(
      "<div style='columns: 2'>"
      "  <svg id='svgroot' style='position: relative'></svg>"
      "</div>");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  EXPECT_FALSE(svgroot->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, HasDescendantWithClipPath) {
  SetBodyInnerHTML(
      "<div id='parent' style='position:relative'>"
      "  <div id='clip-path' style='clip-path: circle(50px at 0 100px)'>"
      "  </div>"
      "</div>");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* clip_path = GetPaintLayerByElementId("clip-path");

  EXPECT_TRUE(parent->HasDescendantWithClipPath());
  EXPECT_FALSE(clip_path->HasDescendantWithClipPath());

  EXPECT_FALSE(parent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(parent->HasVisibleDescendant());
}

TEST_P(PaintLayerTest, HasVisibleDescendant) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='invisible' style='position:relative'>"
      "  <div id='visible' style='visibility: visible; position: relative'>"
      "  </div>"
      "</div>");
  PaintLayer* invisible = GetPaintLayerByElementId("invisible");
  PaintLayer* visible = GetPaintLayerByElementId("visible");

  EXPECT_TRUE(invisible->HasVisibleDescendant());
  EXPECT_FALSE(visible->HasVisibleDescendant());

  EXPECT_FALSE(invisible->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_FALSE(invisible->HasDescendantWithClipPath());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendant) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='parent' style='position:relative; z-index: 0'>"
      "  <div id='child' style='transform: translateZ(1px)'>"
      "  </div>"
      "</div>");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantChangeStyle) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='parent' style='position:relative; z-index: 0'>"
      "  <div id='child' style='position:relative '>"
      "  </div>"
      "</div>");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_FALSE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());

  GetDocument().getElementById("child")->setAttribute(
      HTMLNames::styleAttr, "transform: translateZ(1px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantNotStacking) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='parent' style='position:relative;'>"
      "  <div id='child' style='transform: translateZ(1px)'>"
      "  </div>"
      "</div>");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  // |child| is not a stacking child of |parent|, so it has no 3D transformed
  // descendant.
  EXPECT_FALSE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedGrandchildWithPreserve3d) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='parent' style='position:relative; z-index: 0'>"
      "  <div id='child' style='transform-style: preserve-3d'>"
      "    <div id='grandchild' style='transform: translateZ(1px)'>"
      "    </div>"
      "  </div>"
      "</div>");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  PaintLayer* grandchild = GetPaintLayerByElementId("grandchild");

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_TRUE(child->Has3DTransformedDescendant());
  EXPECT_FALSE(grandchild->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, DescendantDependentFlagsStopsAtThrottledFrames) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='transform' style='transform: translate3d(4px, 5px, 6px);'>"
      "</div>"
      "<iframe id='iframe' sandbox></iframe>");
  SetChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='iframeTransform'"
      "  style='transform: translate3d(4px, 5px, 6px);'/>");

  // Move the child frame offscreen so it becomes available for throttling.
  auto* iframe = toHTMLIFrameElement(GetDocument().getElementById("iframe"));
  iframe->setAttribute(HTMLNames::styleAttr, "transform: translateY(5555px)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Ensure intersection observer notifications get delivered.
  testing::RunPendingTasks();
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_TRUE(ChildDocument().View()->IsHiddenForThrottling());

  {
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    EXPECT_FALSE(GetDocument().View()->ShouldThrottleRendering());
    EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRendering());

    ChildDocument()
        .View()
        ->GetLayoutView()
        ->Layer()
        ->DirtyVisibleContentStatus();

    EXPECT_TRUE(ChildDocument()
                    .View()
                    ->GetLayoutView()
                    ->Layer()
                    ->needs_descendant_dependent_flags_update_);

    // Also check that the rest of the lifecycle succeeds without crashing due
    // to a stale m_needsDescendantDependentFlagsUpdate.
    GetDocument().View()->UpdateAllLifecyclePhases();

    // Still dirty, because the frame was throttled.
    EXPECT_TRUE(ChildDocument()
                    .View()
                    ->GetLayoutView()
                    ->Layer()
                    ->needs_descendant_dependent_flags_update_);
  }

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ChildDocument()
                   .View()
                   ->GetLayoutView()
                   ->Layer()
                   ->needs_descendant_dependent_flags_update_);
}

TEST_P(PaintLayerTest, PaintInvalidationOnNonCompositedScroll) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(
      "<style>* { margin: 0 } ::-webkit-scrollbar { display: none }</style>"
      "<div id='scroller' style='overflow: scroll; width: 50px; height: 50px'>"
      "  <div style='height: 400px'>"
      "    <div id='content-layer' style='position: relative; height: 10px;"
      "        top: 30px; background: blue'>"
      "      <div id='content' style='height: 5px; background: yellow'></div>"
      "    </div>"
      "  </div>"
      "</div>");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutObject* content_layer = GetLayoutObjectByElementId("content-layer");
  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(LayoutRect(0, 30, 50, 10), content_layer->VisualRect());
  EXPECT_EQ(LayoutRect(0, 30, 50, 5), content->VisualRect());

  scroller->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 20),
                                                 kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 10, 50, 10), content_layer->VisualRect());
  EXPECT_EQ(LayoutRect(0, 10, 50, 5), content->VisualRect());
}

TEST_P(PaintLayerTest, PaintInvalidationOnCompositedScroll) {
  EnableCompositing();
  SetBodyInnerHTML(
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

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutObject* content_layer = GetLayoutObjectByElementId("content-layer");
  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(LayoutRect(0, 30, 50, 10), content_layer->VisualRect());
  EXPECT_EQ(LayoutRect(0, 30, 50, 5), content->VisualRect());

  scroller->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 20),
                                                 kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 30, 50, 10), content_layer->VisualRect());
  EXPECT_EQ(LayoutRect(0, 30, 50, 5), content->VisualRect());
}

TEST_P(PaintLayerTest, CompositingContainerStackedFloatUnderStackingInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9'>"
      "      <div id='target' style='float: right; position: relative'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedFloatUnderStackingCompositedInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9; will-change: transform'>"
      "      <div id='target' style='float: right; position: relative'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(span, target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(span,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerNonStackedFloatUnderStackingInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9'>"
      "      <div id='target' style='float: right; overflow: hidden'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
            target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedFloatUnderStackingCompositedInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9; will-change: transform'>"
      "      <div id='target' style='float: right; overflow: hidden'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
            target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedUnderFloatUnderStackingInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9'>"
      "      <div style='float: right'>"
      "        <div id='target' style='position: relative'></div>"
      "      </div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedUnderFloatUnderStackingCompositedInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9; will-change: transform'>"
      "      <div style='float: right'>"
      "        <div id='target' style='position: relative'></div>"
      "      </div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(span, target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(span,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedUnderFloatUnderStackingInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9'>"
      "      <div style='float: right'>"
      "        <div id='target' style='overflow: hidden'></div>"
      "      </div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
            target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedUnderFloatUnderStackingCompositedInline) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='opacity: 0.9; will-change: transform'>"
      "      <div style='float: right'>"
      "        <div id='target' style='overflow: hidden'></div>"
      "      </div>"
      "    </span>"
      "  </div>"
      "</div>");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
            target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, FloatLayerAndAbsoluteUnderInlineLayer) {
  SetBodyInnerHTML(
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

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* absolute = GetPaintLayerByElementId("absolute");
  PaintLayer* span = GetPaintLayerByElementId("span");
  PaintLayer* container = GetPaintLayerByElementId("container");

  EXPECT_EQ(span, floating->Parent());
  EXPECT_EQ(container, floating->ContainingLayer());
  EXPECT_EQ(span, absolute->Parent());
  EXPECT_EQ(span, absolute->ContainingLayer());
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), floating->Location());
  EXPECT_EQ(LayoutPoint(50, 50), absolute->Location());
  EXPECT_EQ(LayoutPoint(133, 133), span->Location());
  EXPECT_EQ(LayoutPoint(20, 20), container->Location());

  EXPECT_EQ(LayoutPoint(-50, -50), floating->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(50, 50), absolute->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), floating->VisualOffsetFromAncestor(container));
  EXPECT_EQ(LayoutPoint(183, 183),
            absolute->VisualOffsetFromAncestor(container));
}

TEST_P(PaintLayerTest, FloatLayerUnderInlineLayerScrolled) {
  SetBodyInnerHTML(
      "<div id='container' style='overflow: scroll; width: 50px; height: 50px'>"
      "  <span id='span' style='position: relative; top: 100px; left: 100px'>"
      "    <div id='floating'"
      "      style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </span>"
      "  <div style='height: 1000px'></div>"
      "</div>");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");
  PaintLayer* container = GetPaintLayerByElementId("container");
  container->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 400),
                                                  kProgrammaticScroll);

  EXPECT_EQ(span, floating->Parent());
  EXPECT_EQ(container, floating->ContainingLayer());
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());

  EXPECT_EQ(LayoutPoint(50, -350), floating->Location());
  EXPECT_EQ(LayoutPoint(100, -300), span->Location());

  EXPECT_EQ(LayoutPoint(-50, -50), floating->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(50, -350),
            floating->VisualOffsetFromAncestor(container));
}

TEST_P(PaintLayerTest, FloatLayerUnderBlockUnderInlineLayer) {
  SetBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div style='display: inline-block; margin: 33px'>"
      "    <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, floating->Parent());
  EXPECT_EQ(span, floating->ContainingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), floating->Location());
  EXPECT_EQ(LayoutPoint(100, 100), span->Location());
  EXPECT_EQ(LayoutPoint(83, 83), floating->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(183, 183), floating->VisualOffsetFromAncestor(
                                       GetDocument().GetLayoutView()->Layer()));
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatUnderInlineLayer) {
  SetBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div style='float: left; margin: 33px'>"
      "    <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, floating->Parent());
  EXPECT_EQ(span->Parent(), floating->ContainingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), floating->Location());
  EXPECT_EQ(LayoutPoint(100, 100), span->Location());
  EXPECT_EQ(LayoutPoint(-17, -17), floating->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), floating->VisualOffsetFromAncestor(
                                     GetDocument().GetLayoutView()->Layer()));
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatLayerUnderInlineLayer) {
  SetBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div id='floatingParent'"
      "      style='float: left; position: relative; margin: 33px'>"
      "    <div id='floating'"
      "        style='float: left; position: relative; top: 50px; left: 50px'>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* floating_parent = GetPaintLayerByElementId("floatingParent");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(floating_parent, floating->Parent());
  EXPECT_EQ(floating_parent, floating->ContainingLayer());
  EXPECT_EQ(span, floating_parent->Parent());
  EXPECT_EQ(span->Parent(), floating_parent->ContainingLayer());

  EXPECT_EQ(LayoutPoint(50, 50), floating->Location());
  EXPECT_EQ(LayoutPoint(33, 33), floating_parent->Location());
  EXPECT_EQ(LayoutPoint(100, 100), span->Location());
  EXPECT_EQ(LayoutPoint(-17, -17), floating->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(-67, -67),
            floating_parent->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), floating->VisualOffsetFromAncestor(
                                     GetDocument().GetLayoutView()->Layer()));
}

TEST_P(PaintLayerTest, LayerUnderFloatUnderInlineLayer) {
  SetBodyInnerHTML(
      "<style>body {margin: 0}</style>"
      "<span id='span' style='position: relative; top: 100px; left: 100px'>"
      "  <div style='float: left; margin: 33px'>"
      "    <div>"
      "      <div id='child' style='position: relative; top: 50px; left: 50px'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</span>");

  PaintLayer* child = GetPaintLayerByElementId("child");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, child->Parent());
  EXPECT_EQ(span->Parent(), child->ContainingLayer());

  EXPECT_EQ(LayoutPoint(83, 83), child->Location());
  EXPECT_EQ(LayoutPoint(100, 100), span->Location());
  EXPECT_EQ(LayoutPoint(-17, -17), child->VisualOffsetFromAncestor(span));
  EXPECT_EQ(LayoutPoint(83, 83), child->VisualOffsetFromAncestor(
                                     GetDocument().GetLayoutView()->Layer()));
}

TEST_P(PaintLayerTest, CompositingContainerFloatingIframe) {
  EnableCompositing();
  SetBodyInnerHTML(
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

  PaintLayer* target = GetPaintLayerByElementId("target");

  // A non-positioned iframe still gets a PaintLayer because PaintLayers are
  // forced for all LayoutEmbeddedContent objects. However, such PaintLayers are
  // not stacked.
  PaintLayer* containing_block = GetPaintLayerByElementId("containingBlock");
  EXPECT_EQ(containing_block, target->CompositingContainer());
  PaintLayer* composited_container =
      GetPaintLayerByElementId("compositedContainer");

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(composited_container,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerSelfPaintingNonStackedFloat) {
  SetBodyInnerHTML(
      "<div id='container' style='position: relative'>"
      "  <span id='span' style='opacity: 0.9'>"
      "    <div id='target' style='columns: 1; float: left'></div>"
      "  </span>"
      "</div>");

  // The target layer is self-painting, but not stacked.
  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_TRUE(target->IsSelfPaintingLayer());
  EXPECT_FALSE(target->StackingNode()->IsStacked());

  PaintLayer* container = GetPaintLayerByElementId("container");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(container, target->ContainingLayer());
  EXPECT_EQ(span, target->CompositingContainer());
}

TEST_P(PaintLayerTest, ColumnSpanLayerUnderExtraLayerScrolled) {
  SetBodyInnerHTML(
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

  PaintLayer* spanner = GetPaintLayerByElementId("spanner");
  PaintLayer* extra_layer = GetPaintLayerByElementId("extraLayer");
  PaintLayer* columns = GetPaintLayerByElementId("columns");
  columns->GetScrollableArea()->SetScrollOffset(ScrollOffset(200, 0),
                                                kProgrammaticScroll);

  EXPECT_EQ(extra_layer, spanner->Parent());
  EXPECT_EQ(columns, spanner->ContainingLayer());
  EXPECT_EQ(columns, extra_layer->Parent()->Parent());
  EXPECT_EQ(columns, extra_layer->ContainingLayer()->Parent());

  EXPECT_EQ(LayoutPoint(-150, 50), spanner->Location());
  EXPECT_EQ(LayoutPoint(100, 100), extra_layer->Location());
  // -60 = 2nd-column-x(40) - scroll-offset-x(200) + x-location(100)
  // 20 = y-location(100) - column-height(80)
  EXPECT_EQ(LayoutPoint(-60, 20),
            extra_layer->VisualOffsetFromAncestor(columns));
  EXPECT_EQ(LayoutPoint(-150, 50), spanner->VisualOffsetFromAncestor(columns));
}

TEST_P(PaintLayerTest, PaintLayerTransformUpdatedOnStyleTransformAnimation) {
  SetBodyInnerHTML("<div id='target' style='will-change: transform'></div>");

  LayoutObject* target_object =
      GetDocument().getElementById("target")->GetLayoutObject();
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target_object)->Layer();
  EXPECT_EQ(nullptr, target_paint_layer->Transform());

  RefPtr<ComputedStyle> old_style =
      ComputedStyle::Clone(target_object->StyleRef());
  ComputedStyle* new_style = target_object->MutableStyle();
  new_style->SetHasCurrentTransformAnimation();
  target_paint_layer->UpdateTransform(old_style.Get(), *new_style);

  EXPECT_NE(nullptr, target_paint_layer->Transform());
}

TEST_P(PaintLayerTest, NeedsRepaintOnSelfPaintingStatusChange) {
  SetBodyInnerHTML(
      "<span id='span' style='opacity: 0.1'>"
      "  <div id='target' style='overflow: hidden; float: left;"
      "      column-width: 10px'>"
      "  </div>"
      "</span>");

  auto* span_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("span"))->Layer();
  auto* target_element = GetDocument().getElementById("target");
  auto* target_object = target_element->GetLayoutObject();
  auto* target_layer = ToLayoutBoxModelObject(target_object)->Layer();

  // Target layer is self painting because it is a multicol container.
  EXPECT_TRUE(target_layer->IsSelfPaintingLayer());
  EXPECT_EQ(span_layer, target_layer->CompositingContainer());
  EXPECT_FALSE(target_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());

  // Removing column-width: 10px makes target layer no longer self-painting,
  // and change its compositing container. The original compositing container
  // span_layer should be marked NeedsRepaint.
  target_element->setAttribute(HTMLNames::styleAttr,
                               "overflow: hidden; float: left");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(target_layer->IsSelfPaintingLayer());
  EXPECT_EQ(span_layer->Parent(), target_layer->CompositingContainer());
  EXPECT_TRUE(target_layer->NeedsRepaint());
  EXPECT_TRUE(target_layer->CompositingContainer()->NeedsRepaint());
  EXPECT_TRUE(span_layer->NeedsRepaint());
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_P(PaintLayerTest, NeedsRepaintOnRemovingStackedLayer) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<style>body {margin-top: 200px; backface-visibility: hidden}</style>"
      "<div id='target' style='position: absolute; top: 0'>Text</div>");

  auto* body = GetDocument().body();
  auto* body_layer = body->GetLayoutBox()->Layer();
  auto* target_element = GetDocument().getElementById("target");
  auto* target_object = target_element->GetLayoutObject();
  auto* target_layer = ToLayoutBoxModelObject(target_object)->Layer();

  // |container| is not the CompositingContainer of |target| because |target|
  // is stacked but |container| is not a stacking context.
  EXPECT_TRUE(target_layer->StackingNode()->IsStacked());
  EXPECT_NE(body_layer, target_layer->CompositingContainer());
  auto* old_compositing_container = target_layer->CompositingContainer();

  body->setAttribute(HTMLNames::styleAttr, "margin-top: 0");
  target_element->setAttribute(HTMLNames::styleAttr, "top: 0");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_FALSE(target_object->HasLayer());
  EXPECT_TRUE(body_layer->NeedsRepaint());
  EXPECT_TRUE(old_compositing_container->NeedsRepaint());

  GetDocument().View()->UpdateAllLifecyclePhases();
}

}  // namespace blink
