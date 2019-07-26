// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::Pointee;

class PaintLayerTest : public PaintTestConfigurations, public RenderingTest {
 public:
  PaintLayerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintLayerTest);

TEST_P(PaintLayerTest, ChildWithoutPaintLayer) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px;'></div>");

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  PaintLayer* root_layer = GetLayoutView().Layer();

  EXPECT_EQ(nullptr, paint_layer);
  EXPECT_NE(nullptr, root_layer);
}

TEST_P(PaintLayerTest, CompositedBoundsAbsPosGrandchild) {
  // BoundingBoxForCompositing is not used in CAP mode.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
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
  EXPECT_EQ(PhysicalRect(0, 0, 150, 150),
            parent_layer->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, CompositedBoundsTransformedChild) {
  // TODO(chrishtr): fix this test for CAP
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id=parent style='overflow: scroll; will-change: transform'>
      <div class='target'
           style='position: relative; transform: skew(-15deg);'>
      </div>
      <div style='width: 1000px; height: 500px; background: lightgray'>
      </div>
    </div>
  )HTML");

  PaintLayer* parent_layer = GetPaintLayerByElementId("parent");
  EXPECT_EQ(PhysicalRect(0, 0, 784, 500),
            parent_layer->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerCompositedBounds) {
  SetBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  EXPECT_EQ(PhysicalRect(0, 0, 800, 600),
            GetLayoutView().Layer()->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerScrollBounds) {
  ScopedOverlayScrollbarsForTest overlay_scrollbars(false);

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
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='background-color: blue; position: absolute;
        width: 110px; height: 120px; top: 40px; left: 60px;
        -webkit-box-reflect: below 3px'>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(PhysicalRect(60, 40, 110, 243),
            layer->PaintingExtent(GetDocument().GetLayoutView()->Layer(),
                                  PhysicalOffset(), 0));
}

TEST_P(PaintLayerTest, PaintingExtentReflectionWithTransform) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='background-color: blue; position: absolute;
        width: 110px; height: 120px; top: 40px; left: 60px;
        -webkit-box-reflect: below 3px; transform: translateX(30px)'>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(PhysicalRect(90, 40, 110, 243),
            layer->PaintingExtent(GetDocument().GetLayoutView()->Layer(),
                                  PhysicalOffset(), 0));
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
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: fixed'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");
  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_FALSE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPosition) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: sticky; top: 0;'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerNoAnchor) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: sticky'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");

  PaintLayer* layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  EXPECT_FALSE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPositionNoScroll) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: sticky; top: 0;'></div>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPositionInsideScroller) {
  SetBodyInnerHTML(R"HTML(
    <div style='overflow:scroll; width: 100px; height: 100px;'>
      <div id='target' style='position: sticky; top: 0;'></div>
      <div style='width: 50px; height: 1000px;'></div>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, CompositedScrollingNoNeedsRepaint) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='scroll' style='width: 100px; height: 100px; overflow: scroll;
        will-change: transform'>
      <div id='content' style='position: relative; background: blue;
          width: 2000px; height: 2000px'></div>
    </div>
  )HTML");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");
  EXPECT_EQ(kPaintsIntoOwnBacking, scroll_layer->GetCompositingState());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  EXPECT_EQ(kNotComposited, content_layer->GetCompositingState());
  EXPECT_EQ(PhysicalOffset(), content_layer->Location());

  scroll_layer->GetScrollableArea()->SetScrollOffset(ScrollOffset(1000, 1000),
                                                     kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(PhysicalOffset(0, 0), content_layer->Location());
  EXPECT_EQ(LayoutSize(1000, 1000),
            content_layer->ContainingLayer()->ScrolledContentOffset());
  EXPECT_FALSE(content_layer->NeedsRepaint());
  EXPECT_FALSE(scroll_layer->NeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, NonCompositedScrollingNeedsRepaint) {
  // CAP scrolling raster invalidation decisions are made in
  // ContentLayerClientImpl::GenerateRasterInvalidations through
  // PaintArtifactCompositor.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='scroll' style='width: 100px; height: 100px; overflow: scroll'>
      <div id='content' style='position: relative; background: blue;
          width: 2000px; height: 2000px'></div>
    </div>
  )HTML");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");
  EXPECT_EQ(kNotComposited, scroll_layer->GetCompositingState());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  EXPECT_EQ(kNotComposited, scroll_layer->GetCompositingState());
  EXPECT_EQ(PhysicalOffset(), content_layer->Location());

  scroll_layer->GetScrollableArea()->SetScrollOffset(ScrollOffset(1000, 1000),
                                                     kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(PhysicalOffset(0, 0), content_layer->Location());
  EXPECT_EQ(LayoutSize(1000, 1000),
            content_layer->ContainingLayer()->ScrolledContentOffset());

  EXPECT_TRUE(scroll_layer->NeedsRepaint());
  EXPECT_FALSE(content_layer->NeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, HasNonIsolatedDescendantWithBlendMode) {
  SetBodyInnerHTML(R"HTML(
    <div id='stacking-grandparent' style='isolation: isolate'>
      <div id='stacking-parent' style='isolation: isolate'>
        <div id='non-stacking-parent' style='position:relative'>
          <div id='blend-mode' style='mix-blend-mode: overlay'>
          </div>
        </div>
      </div>
    </div>
  )HTML");
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

TEST_P(PaintLayerTest, HasStickyPositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child' style='position: sticky'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child->HasStickyPositionDescendant());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child->HasStickyPositionDescendant());
}

TEST_P(PaintLayerTest, HasFixedPositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child' style='position: fixed'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child->HasFixedPositionDescendant());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child->HasFixedPositionDescendant());
}

TEST_P(PaintLayerTest, HasFixedAndStickyPositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child1' style='position: sticky'>
      </div>
      <div id='child2' style='position: fixed'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child1 = GetPaintLayerByElementId("child1");
  PaintLayer* child2 = GetPaintLayerByElementId("child2");
  EXPECT_TRUE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child1->HasFixedPositionDescendant());
  EXPECT_FALSE(child2->HasFixedPositionDescendant());
  EXPECT_TRUE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child1->HasStickyPositionDescendant());
  EXPECT_FALSE(child2->HasStickyPositionDescendant());

  GetDocument().getElementById("child1")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child1->HasFixedPositionDescendant());
  EXPECT_FALSE(child2->HasFixedPositionDescendant());
  EXPECT_FALSE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child1->HasStickyPositionDescendant());
  EXPECT_FALSE(child2->HasStickyPositionDescendant());

  GetDocument().getElementById("child2")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child1->HasFixedPositionDescendant());
  EXPECT_FALSE(child2->HasFixedPositionDescendant());
  EXPECT_FALSE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child1->HasStickyPositionDescendant());
  EXPECT_FALSE(child2->HasStickyPositionDescendant());
}

TEST_P(PaintLayerTest, HasNonContainedAbsolutePositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child' style='position: relative'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: absolute");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());

  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());
}

TEST_P(PaintLayerTest, HasSelfPaintingDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position: relative'>
      <div id='child' style='position: relative'>
        <div></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->HasSelfPaintingLayerDescendant());
  EXPECT_FALSE(child->HasSelfPaintingLayerDescendant());
}

TEST_P(PaintLayerTest, HasSelfPaintingDescendantNotSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position: relative'>
      <div id='child' style='overflow: auto'>
        <div></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_FALSE(parent->HasSelfPaintingLayerDescendant());
  EXPECT_FALSE(child->HasSelfPaintingLayerDescendant());
}

TEST_P(PaintLayerTest, HasSelfPaintingParentNotSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='overflow: auto'>
      <div id='child' style='position: relative'>
        <div></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->HasSelfPaintingLayerDescendant());
  EXPECT_FALSE(child->HasSelfPaintingLayerDescendant());
}

static const Vector<PaintLayer*>* LayersPaintingOverlayScrollbarsAfter(
    const PaintLayer* layer) {
  return PaintLayerPaintOrderIterator(*layer->AncestorStackingContext(),
                                      kPositiveZOrderChildren)
      .LayersPaintingOverlayScrollbarsAfter(layer);
}

TEST_P(PaintLayerTest, ReorderOverlayScrollbars_StackedWithInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='overflow: auto; position: relative;
                            width: 100px; height: 100px'>
      <div id='child' style='position: relative; height: 200px'></div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_StackedWithOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>#child { width: 200px; height: 200px; }</style>
    <div id='parent' style='overflow: auto; position: relative; height: 100px'>
      <div id='child' style='position: absolute'></div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: absolute");
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(PaintLayerTest, ReorderOverlayScrollbars_StackedWithZIndexDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>#child { position: absolute; width: 200px; height: 200px; }</style>
    <div id='parent' style='overflow: auto; position: relative; height: 100px'>
      <div id='child' style='position: absolute; z-index: 1;
                             width: 200px; height: 200px'></div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: -1");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: 2");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NestedStackedWithInFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'
         style='overflow: auto; position: relative; height: 100px'>
      <div id='parent' style='overflow: auto; height: 200px'>
        <div id="child" style='position: relative; height: 300px'></div>
      </div>
    </div>
  )HTML");
  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayScrollbars());
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent, ancestor)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NestedStackedWithOutOfFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'
         style='overflow: auto; position: relative; height: 100px'>
      <div id='parent' style='overflow: auto; position: absolute;
                             width: 200px; height: 200px'>
        <div id="child" style='position: absolute; width: 300px; height: 300px'>
        </div>
      </div>
    </div>
  )HTML");
  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayScrollbars());
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent, ancestor)));
}

TEST_P(PaintLayerTest, ReorderOverlayScrollbars_MultipleChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { width: 200px; height: 200px; }
      #parent { overflow: auto; width: 100px; height: 100px; }
    </style>
    <div id='parent'>
      <div id="low-child" style='position: absolute; z-index: 1'></div>
      <div id="middle-child" style='position: relative; z-index: 2'></div>
      <div id="high-child" style='position: absolute; z-index: 3'></div>
    </div>
  )HTML");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* low_child = GetPaintLayerByElementId("low-child");
  auto* middle_child = GetPaintLayerByElementId("middle-child");
  auto* high_child = GetPaintLayerByElementId("high-child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(low_child));
  // The highest contained child by parent is middle_child because the
  // absolute-position children are not contained.
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(middle_child),
              Pointee(ElementsAre(parent)));
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(high_child));

  GetDocument().getElementById("parent")->setAttribute(
      html_names::kStyleAttr, "position: absolute; z-index: 1");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(low_child));
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(middle_child));
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(high_child));

  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       "position: absolute");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(low_child));
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(middle_child));
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(high_child),
              Pointee(ElementsAre(parent)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NonStackedWithInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='overflow: auto; width: 100px; height: 100px'>
      <div id='child' style='position: relative; height: 200px'></div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NonStackedWithZIndexInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>#child { position: relative; height: 200px; }</style>
    <div id='parent' style='overflow: auto; height: 100px'>
      <div id='child' style='z-index: 1'>
      </div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: -1");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: 2");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NonStackedWithOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='overflow: auto; height: 100px'>
      <div id='child' style='position: absolute;
                             width: 200px; height: 200px'></div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NonStackedWithNonStackedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='overflow: auto'>
      <div id='child' style='overflow: auto'></div>
    </div>
  )HTML");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NestedNonStackedWithInFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='overflow: auto; height: 100px'>
      <div id='parent' style='overflow: auto; height: 200px'>
        <div id="child" style='position: relative; height: 300px'></div>
      </div>
    </div>
  )HTML");
  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayScrollbars());
  EXPECT_TRUE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_THAT(LayersPaintingOverlayScrollbarsAfter(child),
              Pointee(ElementsAre(parent, ancestor)));
}

TEST_P(PaintLayerTest,
       ReorderOverlayScrollbars_NestedNonStackedWithOutOfFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='overflow: auto; height: 100px'>
      <div id='parent' style='overflow: auto; height: 200px'>
        <div id="child" style='position: absolute; width: 300px; height: 300px'>
        </div>
      </div>
    </div>
  )HTML");
  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(ancestor->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(parent->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(child->NeedsReorderOverlayScrollbars());
  EXPECT_FALSE(LayersPaintingOverlayScrollbarsAfter(child));
}

TEST_P(PaintLayerTest, SubsequenceCachingStackingContexts) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative'>
      <div id='child1' style='position: relative'>
        <div id='grandchild1' style='position: relative'></div>
      </div>
      <div id='child2' style='isolation: isolate'>
        <div id='grandchild2' style='position: relative'></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child1 = GetPaintLayerByElementId("child1");
  PaintLayer* child2 = GetPaintLayerByElementId("child2");
  PaintLayer* grandchild1 = GetPaintLayerByElementId("grandchild1");
  PaintLayer* grandchild2 = GetPaintLayerByElementId("grandchild2");

  EXPECT_FALSE(parent->SupportsSubsequenceCaching());
  EXPECT_FALSE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild1->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild2->SupportsSubsequenceCaching());

  GetDocument()
      .getElementById("grandchild1")
      ->setAttribute(html_names::kStyleAttr, "isolation: isolate");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->SupportsSubsequenceCaching());
  EXPECT_FALSE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_TRUE(grandchild1->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild2->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingSVGRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position: relative'>
      <svg id='svgroot' style='position: relative'></svg>
    </div>
  )HTML");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  EXPECT_TRUE(svgroot->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingMuticol) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2'>
      <svg id='svgroot' style='position: relative'></svg>
    </div>
  )HTML");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  EXPECT_FALSE(svgroot->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, NegativeZIndexChangeToPositive) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #child { position: relative; }
    </style>
    <div id='target' style='isolation: isolate'>
      <div id='child' style='z-index: -1'></div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");

  EXPECT_TRUE(
      PaintLayerPaintOrderIterator(*target, kNegativeZOrderChildren).Next());
  EXPECT_FALSE(
      PaintLayerPaintOrderIterator(*target, kPositiveZOrderChildren).Next());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: 1");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      PaintLayerPaintOrderIterator(*target, kNegativeZOrderChildren).Next());
  EXPECT_TRUE(
      PaintLayerPaintOrderIterator(*target, kPositiveZOrderChildren).Next());
}

TEST_P(PaintLayerTest, HasDescendantWithClipPath) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative'>
      <div id='clip-path' style='clip-path: circle(50px at 0 100px)'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* clip_path = GetPaintLayerByElementId("clip-path");

  EXPECT_TRUE(parent->HasDescendantWithClipPath());
  EXPECT_FALSE(clip_path->HasDescendantWithClipPath());

  EXPECT_FALSE(parent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(parent->HasVisibleDescendant());
}

TEST_P(PaintLayerTest, HasVisibleDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='invisible' style='position:relative'>
      <div id='visible' style='visibility: visible; position: relative'>
      </div>
    </div>
  )HTML");
  PaintLayer* invisible = GetPaintLayerByElementId("invisible");
  PaintLayer* visible = GetPaintLayerByElementId("visible");

  EXPECT_TRUE(invisible->HasVisibleDescendant());
  EXPECT_FALSE(visible->HasVisibleDescendant());

  EXPECT_FALSE(invisible->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_FALSE(invisible->HasDescendantWithClipPath());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative; z-index: 0'>
      <div id='child' style='transform: translateZ(1px)'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantChangeStyle) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative; z-index: 0'>
      <div id='child' style='position:relative '>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_FALSE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "transform: translateZ(1px)");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantNotStacking) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative;'>
      <div id='child' style='transform: translateZ(1px)'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  // |child| is not a stacking child of |parent|, so it has no 3D transformed
  // descendant.
  EXPECT_FALSE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedGrandchildWithPreserve3d) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative; z-index: 0'>
      <div id='child' style='transform-style: preserve-3d'>
        <div id='grandchild' style='transform: translateZ(1px)'>
        </div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  PaintLayer* grandchild = GetPaintLayerByElementId("grandchild");

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_TRUE(child->Has3DTransformedDescendant());
  EXPECT_FALSE(grandchild->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, DescendantDependentFlagsStopsAtThrottledFrames) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='transform' style='transform: translate3d(4px, 5px, 6px);'>
    </div>
    <iframe id='iframe' sandbox></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='iframeTransform'
      style='transform: translate3d(4px, 5px, 6px);'/>
  )HTML");

  // Move the child frame offscreen so it becomes available for throttling.
  auto* iframe = ToHTMLIFrameElement(GetDocument().getElementById("iframe"));
  iframe->setAttribute(html_names::kStyleAttr, "transform: translateY(5555px)");
  UpdateAllLifecyclePhasesForTest();
  // Ensure intersection observer notifications get delivered.
  test::RunPendingTasks();
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
    UpdateAllLifecyclePhasesForTest();

    // Still dirty, because the frame was throttled.
    EXPECT_TRUE(ChildDocument()
                    .View()
                    ->GetLayoutView()
                    ->Layer()
                    ->needs_descendant_dependent_flags_update_);
  }

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ChildDocument()
                   .View()
                   ->GetLayoutView()
                   ->Layer()
                   ->needs_descendant_dependent_flags_update_);
}

TEST_P(PaintLayerTest, PaintInvalidationOnNonCompositedScroll) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>* { margin: 0 } ::-webkit-scrollbar { display: none }</style>
    <div id='scroller' style='overflow: scroll; width: 50px; height: 50px'>
      <div style='height: 400px'>
        <div id='content-layer' style='position: relative; height: 10px;
            top: 30px; background: blue'>
          <div id='content' style='height: 5px; background: yellow'></div>
        </div>
      </div>
    </div>
  )HTML");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutObject* content_layer = GetLayoutObjectByElementId("content-layer");
  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(IntRect(0, 30, 50, 10),
            content_layer->FirstFragment().VisualRect());
  EXPECT_EQ(IntRect(0, 30, 50, 5), content->FirstFragment().VisualRect());

  scroller->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 20),
                                                 kProgrammaticScroll);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 30, 50, 10),
            content_layer->FirstFragment().VisualRect());
  EXPECT_EQ(IntRect(0, 30, 50, 5), content->FirstFragment().VisualRect());
}

TEST_P(PaintLayerTest, PaintInvalidationOnCompositedScroll) {
  SetBodyInnerHTML(R"HTML(
    <style>* { margin: 0 } ::-webkit-scrollbar { display: none }</style>
    <div id='scroller' style='overflow: scroll; width: 50px; height: 50px;
        will-change: transform'>
      <div style='height: 400px'>
        <div id='content-layer' style='position: relative; height: 10px;
            top: 30px; background: blue'>
          <div id='content' style='height: 5px; background: yellow'></div>
        </div>
      </div>
    </div>
  )HTML");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutObject* content_layer = GetLayoutObjectByElementId("content-layer");
  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(IntRect(0, 30, 50, 10),
            content_layer->FirstFragment().VisualRect());
  EXPECT_EQ(IntRect(0, 30, 50, 5), content->FirstFragment().VisualRect());

  scroller->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 20),
                                                 kProgrammaticScroll);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 30, 50, 10),
            content_layer->FirstFragment().VisualRect());
  EXPECT_EQ(IntRect(0, 30, 50, 5), content->FirstFragment().VisualRect());
}

TEST_P(PaintLayerTest, CompositingContainerStackedFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div id='target' style='float: right; position: relative'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div id='target' style='float: right; position: relative'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(span, target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(span,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerNonStackedFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div id='target' style='float: right; overflow: hidden'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div id='target' style='float: right; overflow: hidden'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
      EXPECT_EQ(GetPaintLayerByElementId("span"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    } else {
      EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    }
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedUnderFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div style='float: right'>
            <div id='target' style='position: relative'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedUnderFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div style='float: right'>
            <div id='target' style='position: relative'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(span, target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(span,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedUnderFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div style='float: right'>
            <div id='target' style='overflow: hidden'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedUnderFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div style='float: right'>
            <div id='target' style='overflow: hidden'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
      EXPECT_EQ(GetPaintLayerByElementId("span"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    } else {
      EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    }
  }
}

TEST_P(PaintLayerTest, FloatLayerAndAbsoluteUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position: absolute; top: 20px; left: 20px'>
      <div style='margin: 33px'>
        <span id='span' style='position: relative; top: 100px; left: 100px'>
          <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
          </div>
          <div id='absolute'
            style='position: absolute; top: 50px; left: 50px'>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* absolute = GetPaintLayerByElementId("absolute");
  PaintLayer* span = GetPaintLayerByElementId("span");
  PaintLayer* container = GetPaintLayerByElementId("container");

  EXPECT_EQ(span, floating->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating->ContainingLayer());
  } else {
    EXPECT_EQ(container, floating->ContainingLayer());
  }
  EXPECT_EQ(span, absolute->Parent());
  EXPECT_EQ(span, absolute->ContainingLayer());
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(0, 0), floating->Location());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
  } else {
    EXPECT_EQ(PhysicalOffset(33, 33), floating->Location());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
  }
  EXPECT_EQ(PhysicalOffset(50, 50), absolute->Location());
  EXPECT_EQ(PhysicalOffset(33, 33), span->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            span->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(PhysicalOffset(20, 20), container->Location());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(50, 50), floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(container));
  } else {
    EXPECT_EQ(PhysicalOffset(-50, -50),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              floating->VisualOffsetFromAncestor(container));
  }
  EXPECT_EQ(PhysicalOffset(50, 50), absolute->VisualOffsetFromAncestor(span));
  EXPECT_EQ(PhysicalOffset(183, 183),
            absolute->VisualOffsetFromAncestor(container));
}

TEST_P(PaintLayerTest, FloatLayerUnderInlineLayerScrolled) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='overflow: scroll; width: 50px; height: 50px'>
      <span id='span' style='position: relative; top: 100px; left: 100px'>
        <div id='floating'
          style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </span>
      <div style='height: 1000px'></div>
    </div>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");
  PaintLayer* container = GetPaintLayerByElementId("container");
  container->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 400),
                                                  kProgrammaticScroll);

  EXPECT_EQ(span, floating->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating->ContainingLayer());
  } else {
    EXPECT_EQ(container, floating->ContainingLayer());
  }
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());

  EXPECT_EQ(PhysicalOffset(0, 0), span->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            span->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(LayoutSize(0, 400),
            span->ContainingLayer()->ScrolledContentOffset());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(0, 0), floating->Location());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(50, 50), floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(150, -250),
              floating->VisualOffsetFromAncestor(container));
  } else {
    EXPECT_EQ(PhysicalOffset(0, 0), floating->Location());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(LayoutSize(0, 400),
              floating->ContainingLayer()->ScrolledContentOffset());
    EXPECT_EQ(PhysicalOffset(-50, -50),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(50, -350),
              floating->VisualOffsetFromAncestor(container));
  }
}

TEST_P(PaintLayerTest, FloatLayerUnderBlockUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div style='display: inline-block; margin: 33px'>
        <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, floating->Parent());
  EXPECT_EQ(span, floating->ContainingLayer());

  EXPECT_EQ(PhysicalOffset(33, 33), floating->Location());
  EXPECT_EQ(PhysicalOffset(50, 50),
            floating->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(PhysicalOffset(0, 0), span->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            span->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(PhysicalOffset(83, 83), floating->VisualOffsetFromAncestor(span));
  EXPECT_EQ(PhysicalOffset(183, 183),
            floating->VisualOffsetFromAncestor(
                GetDocument().GetLayoutView()->Layer()));
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div style='float: left; margin: 33px'>
        <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, floating->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating->ContainingLayer());
  } else {
    EXPECT_EQ(span->Parent(), floating->ContainingLayer());
  }

  EXPECT_EQ(PhysicalOffset(33, 33), floating->Location());
  EXPECT_EQ(PhysicalOffset(50, 50),
            floating->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(PhysicalOffset(0, 0), span->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            span->GetLayoutObject().OffsetForInFlowPosition());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(83, 83), floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  } else {
    EXPECT_EQ(PhysicalOffset(-17, -17),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatLayerUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div id='floatingParent'
          style='float: left; position: relative; margin: 33px'>
        <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* floating_parent = GetPaintLayerByElementId("floatingParent");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(floating_parent, floating->Parent());
  EXPECT_EQ(floating_parent, floating->ContainingLayer());
  EXPECT_EQ(span, floating_parent->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating_parent->ContainingLayer());
  } else {
    EXPECT_EQ(span->Parent(), floating_parent->ContainingLayer());
  }

  EXPECT_EQ(PhysicalOffset(0, 0), floating->Location());
  EXPECT_EQ(PhysicalOffset(50, 50),
            floating->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(PhysicalOffset(33, 33), floating_parent->Location());
  EXPECT_EQ(PhysicalOffset(0, 0), span->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            span->GetLayoutObject().OffsetForInFlowPosition());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(83, 83), floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(33, 33),
              floating_parent->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  } else {
    EXPECT_EQ(PhysicalOffset(-17, -17),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(-67, -67),
              floating_parent->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, LayerUnderFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div style='float: left; margin: 33px'>
        <div>
          <div id='child' style='position: relative; top: 50px; left: 50px'>
          </div>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* child = GetPaintLayerByElementId("child");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, child->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, child->ContainingLayer());
  } else {
    EXPECT_EQ(span->Parent(), child->ContainingLayer());
  }

  EXPECT_EQ(PhysicalOffset(33, 33), child->Location());
  EXPECT_EQ(PhysicalOffset(50, 50),
            child->GetLayoutObject().OffsetForInFlowPosition());
  EXPECT_EQ(PhysicalOffset(0, 0), span->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            span->GetLayoutObject().OffsetForInFlowPosition());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(83, 83), child->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              child->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));

  } else {
    EXPECT_EQ(PhysicalOffset(-17, -17), child->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              child->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, CompositingContainerFloatingIframe) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <div style='backface-visibility: hidden'></div>
        <span id='span'
            style='clip-path: polygon(0px 15px, 0px 54px, 100px 0px)'>
          <iframe srcdoc='foo' id='target' style='float: right'></iframe>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");

  // A non-positioned iframe still gets a PaintLayer because PaintLayers are
  // forced for all LayoutEmbeddedContent objects. However, such PaintLayers are
  // not stacked.
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }
  PaintLayer* composited_container =
      GetPaintLayerByElementId("compositedContainer");

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(composited_container,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerSelfPaintingNonStackedFloat) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position: relative'>
      <span id='span' style='opacity: 0.9'>
        <div id='target' style='columns: 1; float: left'></div>
      </span>
    </div>
  )HTML");

  // The target layer is self-painting, but not stacked.
  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_TRUE(target->IsSelfPaintingLayer());
  EXPECT_FALSE(target->GetLayoutObject().StyleRef().IsStacked());

  PaintLayer* container = GetPaintLayerByElementId("container");
  PaintLayer* span = GetPaintLayerByElementId("span");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, target->ContainingLayer());
  } else {
    EXPECT_EQ(container, target->ContainingLayer());
  }
  EXPECT_EQ(span, target->CompositingContainer());
}

TEST_P(PaintLayerTest, ColumnSpanLayerUnderExtraLayerScrolled) {
  SetBodyInnerHTML(R"HTML(
    <div id='columns' style='overflow: hidden; width: 80px; height: 80px;
        columns: 2; column-gap: 0'>
      <div id='extraLayer'
          style='position: relative; top: 100px; left: 100px'>
        <div id='spanner' style='column-span: all; position: relative;
            top: 50px; left: 50px'>
        </div>
      </div>
      <div style='height: 1000px'></div>
    </div>
  )HTML");

  PaintLayer* spanner = GetPaintLayerByElementId("spanner");
  PaintLayer* extra_layer = GetPaintLayerByElementId("extraLayer");
  PaintLayer* columns = GetPaintLayerByElementId("columns");
  columns->GetScrollableArea()->SetScrollOffset(ScrollOffset(200, 0),
                                                kProgrammaticScroll);

  EXPECT_EQ(extra_layer, spanner->Parent());
  EXPECT_EQ(columns, spanner->ContainingLayer());
  EXPECT_EQ(columns, extra_layer->Parent()->Parent());
  EXPECT_EQ(columns, extra_layer->ContainingLayer()->Parent());

  EXPECT_EQ(PhysicalOffset(0, 0), spanner->Location());
  EXPECT_EQ(PhysicalOffset(50, 50),
            spanner->GetLayoutObject().OffsetForInFlowPosition());

  EXPECT_EQ(LayoutSize(200, 0),
            spanner->ContainingLayer()->ScrolledContentOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), extra_layer->Location());
  EXPECT_EQ(PhysicalOffset(100, 100),
            extra_layer->GetLayoutObject().OffsetForInFlowPosition());
  // -60 = 2nd-column-x(40) - scroll-offset-x(200) + x-location(100)
  // 20 = y-location(100) - column-height(80)
  EXPECT_EQ(PhysicalOffset(-60, 20),
            extra_layer->VisualOffsetFromAncestor(columns));
  EXPECT_EQ(PhysicalOffset(-150, 50),
            spanner->VisualOffsetFromAncestor(columns));
}

TEST_P(PaintLayerTest, PaintLayerTransformUpdatedOnStyleTransformAnimation) {
  SetBodyInnerHTML("<div id='target' style='will-change: transform'></div>");

  LayoutObject* target_object =
      GetDocument().getElementById("target")->GetLayoutObject();
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target_object)->Layer();
  EXPECT_EQ(nullptr, target_paint_layer->Transform());

  const ComputedStyle* old_style = target_object->Style();
  scoped_refptr<ComputedStyle> new_style = ComputedStyle::Clone(*old_style);
  new_style->SetHasCurrentTransformAnimation(true);
  target_paint_layer->UpdateTransform(old_style, *new_style);

  EXPECT_NE(nullptr, target_paint_layer->Transform());
}

TEST_P(PaintLayerTest, NeedsRepaintOnSelfPaintingStatusChange) {
  SetBodyInnerHTML(R"HTML(
    <span id='span' style='opacity: 0.1'>
      <div id='target' style='overflow: hidden; float: left;
          column-width: 10px'>
      </div>
    </span>
  )HTML");

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
  target_element->setAttribute(html_names::kStyleAttr,
                               "overflow: hidden; float: left");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // TODO(yosin): Once multicol in LayoutNG, we should remove following
  // assignments. This is because the layout tree maybe reattached. In LayoutNG
  // phase 1, layout tree is reattached because multicol forces legacy layout.
  target_object = target_element->GetLayoutObject();
  target_layer = ToLayoutBoxModelObject(target_object)->Layer();
  EXPECT_FALSE(target_layer->IsSelfPaintingLayer());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span_layer, target_layer->CompositingContainer());
  } else {
    EXPECT_EQ(span_layer->Parent(), target_layer->CompositingContainer());
  }
  EXPECT_TRUE(target_layer->NeedsRepaint());
  EXPECT_TRUE(target_layer->CompositingContainer()->NeedsRepaint());
  EXPECT_TRUE(span_layer->NeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, NeedsRepaintOnRemovingStackedLayer) {
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
  EXPECT_TRUE(target_layer->GetLayoutObject().StyleRef().IsStacked());
  EXPECT_NE(body_layer, target_layer->CompositingContainer());
  auto* old_compositing_container = target_layer->CompositingContainer();

  body->setAttribute(html_names::kStyleAttr, "margin-top: 0");
  target_element->setAttribute(html_names::kStyleAttr, "top: 0");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_FALSE(target_object->HasLayer());
  EXPECT_TRUE(body_layer->NeedsRepaint());
  EXPECT_TRUE(old_compositing_container->NeedsRepaint());

  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, FrameViewContentSize) {
  SetBodyInnerHTML(
      "<style> body { width: 1200px; height: 900px; margin: 0 } </style>");
  EXPECT_EQ(IntSize(800, 600), GetDocument().View()->Size());
}

TEST_P(PaintLayerTest, ReferenceClipPathWithPageZoom) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div style='width: 200px; height: 200px; background-color: blue;
                clip-path: url(#clip)' id='content'></div>
    <svg>
      <clipPath id='clip'>
        <path d='M50,50h100v100h-100z'/>
      </clipPath>
    </svg>
  )HTML");

  auto* content = GetDocument().getElementById("content");
  auto* body = GetDocument().body();

  // A hit test on the content div within the clip should hit it.
  EXPECT_EQ(content, GetDocument().ElementFromPoint(125, 75));
  EXPECT_EQ(content, GetDocument().ElementFromPoint(75, 125));

  // A hit test on the content div outside the clip should not hit it.
  EXPECT_EQ(body, GetDocument().ElementFromPoint(151, 60));
  EXPECT_EQ(body, GetDocument().ElementFromPoint(60, 151));

  // Zoom the page by 2x,
  GetDocument().GetFrame()->SetPageZoomFactor(2);

  // A hit test on the content div within the clip should hit it.
  EXPECT_EQ(content, GetDocument().ElementFromPoint(125, 75));
  EXPECT_EQ(content, GetDocument().ElementFromPoint(75, 125));

  // A hit test on the content div outside the clip should not hit it.
  EXPECT_EQ(body, GetDocument().ElementFromPoint(151, 60));
  EXPECT_EQ(body, GetDocument().ElementFromPoint(60, 151));
}

TEST_P(PaintLayerTest, FragmentedHitTest) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      break-inside: avoid-column;
      width: 50px;
      height: 50px;
      position: relative;
    }
    </style>
    <ul style="column-count: 4; position: relative">
      <div></div>
      <div id=target style=" position: relative; transform: translateY(0px);">
      </div>
    </ul>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  EXPECT_EQ(target, GetDocument().ElementFromPoint(280, 30));
}

TEST_P(PaintLayerTest, SquashingOffsets) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SetHtmlInnerHTML(R"HTML(
    <style>
      * { margin: 0 }
    </style>
    <div id=target
        style='width: 200px; height: 200px; position: relative; will-change: transform'></div>
    <div id=squashed
        style='width: 200px; height: 200px; top: -200px; position: relative;'></div>
    <div style='width: 10px; height: 3000px'></div>
  )HTML");

  auto* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());
  PhysicalOffset point;
  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      squashed->GetLayoutObject(), point);
  EXPECT_EQ(PhysicalOffset(), point);

  EXPECT_EQ(PhysicalOffset(), squashed->ComputeOffsetFromAncestor(
                                  squashed->TransformAncestorOrRoot()));

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 25),
                                                   kUserScroll);
  UpdateAllLifecyclePhasesForTest();

  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      squashed->GetLayoutObject(), point);
  EXPECT_EQ(PhysicalOffset(), point);

  EXPECT_EQ(PhysicalOffset(), squashed->ComputeOffsetFromAncestor(
                                  squashed->TransformAncestorOrRoot()));
}

TEST_P(PaintLayerTest, HitTestWithIgnoreClipping) {
  SetBodyInnerHTML("<div id='hit' style='width: 90px; height: 9000px;'></div>");

  HitTestRequest request(HitTestRequest::kIgnoreClipping);
  // (10, 900) is outside the viewport clip of 800x600.
  HitTestLocation location((IntPoint(10, 900)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(GetDocument().getElementById("hit"), result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <div id='hit' style='width: 100px; height: 100px;'>
      <div id='child' style='width:100px;height:100px'></div>
    </div>
    <div id='overlap' style='position:relative;top:-50px;width:100px;height:100px'></div>
  )HTML");
  Element* hit = GetDocument().getElementById("hit");
  Element* child = GetDocument().getElementById("child");
  Element* overlap = GetDocument().getElementById("overlap");

  // Regular hit test over 'child'
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(50, 25)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(child, result.InnerNode());

  // Same hit test, with stop node.
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           hit->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(hit, result.InnerNode());

  // Regular hit test over 'overlap'
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  location = HitTestLocation((PhysicalOffset(50, 75)));
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(overlap, result.InnerNode());

  // Same hit test, with stop node, should still hit 'overlap' because it's not
  // a descendant of 'hit'.
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           hit->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(overlap, result.InnerNode());

  // List-based hit test with stop node
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                               HitTestRequest::kListBased,
                           hit->GetLayoutObject());
  location = HitTestLocation((PhysicalRect(40, 15, 20, 20)));
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(1u, result.ListBasedTestResult().size());
  EXPECT_EQ(hit, *result.ListBasedTestResult().begin());
}

TEST_P(PaintLayerTest, HitTestTableWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .cell {
      width: 100px;
      height: 100px;
    }
    </style>
    <table id='table'>
      <tr>
        <td><div id='cell11' class='cell'></td>
        <td><div id='cell12' class='cell'></td>
      </tr>
      <tr>
        <td><div id='cell21' class='cell'></td>
        <td><div id='cell22' class='cell'></td>
      </tr>
    </table>
    )HTML");
  Element* table = GetDocument().getElementById("table");
  Element* cell11 = GetDocument().getElementById("cell11");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(50, 50)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(cell11, result.InnerNode());

  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           table->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(table, result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestSVGWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style='width:100px;height:100px' viewBox='0 0 100 100'>
      <circle id='circle' cx='50' cy='50' r='50' />
    </svg>
    )HTML");
  Element* svg = GetDocument().getElementById("svg");
  Element* circle = GetDocument().getElementById("circle");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(50, 50)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(circle, result.InnerNode());

  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           svg->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(svg, result.InnerNode());
}

TEST_P(PaintLayerTest, SetNeedsRepaintSelfPaintingUnderNonSelfPainting) {
  SetHtmlInnerHTML(R"HTML(
    <span id='span' style='opacity: 0.5'>
      <div id='floating' style='float: left; overflow: hidden'>
        <div id='multicol' style='columns: 2'>A</div>
      </div>
    </span>
  )HTML");

  auto* html_layer =
      ToLayoutBoxModelObject(GetDocument().documentElement()->GetLayoutObject())
          ->Layer();
  auto* span_layer = GetPaintLayerByElementId("span");
  auto* floating_layer = GetPaintLayerByElementId("floating");
  auto* multicol_layer = GetPaintLayerByElementId("multicol");
  EXPECT_FALSE(html_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());
  EXPECT_FALSE(floating_layer->NeedsRepaint());
  EXPECT_FALSE(multicol_layer->NeedsRepaint());

  multicol_layer->SetNeedsRepaint();
  EXPECT_TRUE(html_layer->NeedsRepaint());
  EXPECT_TRUE(span_layer->NeedsRepaint());
  EXPECT_TRUE(floating_layer->NeedsRepaint());
  EXPECT_TRUE(multicol_layer->NeedsRepaint());
}

TEST_P(PaintLayerTest, HitTestPseudoElementWithContinuation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #target::before {
        content: ' ';
        display: block;
        height: 100px
      }
    </style>
    <span id='target'></span>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(target->GetPseudoElement(kPseudoIdBefore),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestFirstLetterPseudoElement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { height: 100px; }
      #container::first-letter { font-size: 50px; }
    </style>
    <div id='container'>
      <div>
        <span id='target'>First letter</span>
      </div>
    </div>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  Element* container = GetDocument().getElementById("container");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(container->GetPseudoElement(kPseudoIdFirstLetter),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestFirstLetterInBeforePseudoElement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { height: 100px; }
      #container::first-letter { font-size: 50px; }
      #target::before { content: "First letter"; }
    </style>
    <div id='container'>
      <div>
        <span id='target'></span>
      </div>
    </div>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  Element* container = GetDocument().getElementById("container");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(container->GetPseudoElement(kPseudoIdFirstLetter),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestFloatInsideInlineBoxContainer) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { font: 10px/10px Ahem; width: 70px; }
      #inline-container { border: 1px solid black; }
      #target { float: right; }
    </style>
    <div id='container'>
      <span id='inline-container'>
        <a href='#' id='target'>bar</a>
        foo
      </span>
    </div>
  )HTML");
  Node* target = GetDocument().getElementById("target")->firstChild();
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(55, 5));  // At the center of "bar"
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestFirstLetterPseudoElementDisplayContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { height: 100px; }
      #container::first-letter { font-size: 50px; }
      #target { display: contents; }
    </style>
    <div id='container'>
      <div>
        <span id='target'>First letter</span>
      </div>
    </div>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  Element* container = GetDocument().getElementById("container");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(container->GetPseudoElement(kPseudoIdFirstLetter),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, BackgroundIsKnownToBeOpaqueInRectChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        position: relative;
        isolation: isolate;
      }
    </style>
    <div id='target'>
      <div style='background: blue'></div>
    </div>
  )HTML");

  PaintLayer* target_layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(target_layer->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 100, 100), true));
  EXPECT_FALSE(target_layer->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 100, 100), false));
}

TEST_P(PaintLayerTest,
       ChangeAlphaNeedsCompositingInputsAndPaintPropertyUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        background: white;
        width: 100px;
        height: 100px;
        position: relative;
      }
    </style>
    <div id='target'>
    </div>
  )HTML");
  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_FALSE(target->NeedsCompositingInputsUpdate());
  EXPECT_FALSE(target->GetLayoutObject().NeedsPaintPropertyUpdate());
  EXPECT_FALSE(target->Parent()->GetLayoutObject().NeedsPaintPropertyUpdate());

  StyleDifference diff;
  diff.SetHasAlphaChanged();
  target->StyleDidChange(diff, target->GetLayoutObject().Style());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    EXPECT_FALSE(target->NeedsCompositingInputsUpdate());
  else
    EXPECT_TRUE(target->NeedsCompositingInputsUpdate());
  EXPECT_TRUE(target->GetLayoutObject().NeedsPaintPropertyUpdate());
  // See the TODO in PaintLayer::SetNeedsCompositingInputsUpdate().
  EXPECT_TRUE(target->Parent()->GetLayoutObject().NeedsPaintPropertyUpdate());
}

TEST_P(PaintLayerTest, PaintLayerCommonAncestor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: relative;
      }
    </style>
    <div id='wrapper'>
      <div id='target1'>
        <div id='target1x1'></div>
      </div>
      <div id='target2'></div>
      <div>
        <div id='target3'></div>
      </div>
    </div>
  )HTML");

  PaintLayer* wrapper = GetPaintLayerByElementId("wrapper");
  PaintLayer* target1 = GetPaintLayerByElementId("target1");
  PaintLayer* target1x1 = GetPaintLayerByElementId("target1x1");
  PaintLayer* target2 = GetPaintLayerByElementId("target2");
  PaintLayer* target3 = GetPaintLayerByElementId("target3");

  EXPECT_EQ(target1->CommonAncestor(target1), target1);
  EXPECT_EQ(target1->CommonAncestor(target1x1), target1);
  EXPECT_EQ(target1->CommonAncestor(target2), wrapper);
  EXPECT_EQ(target1->CommonAncestor(target3), wrapper);

  EXPECT_EQ(target1x1->CommonAncestor(target1), target1);
  EXPECT_EQ(target1x1->CommonAncestor(target1x1), target1x1);
  EXPECT_EQ(target1x1->CommonAncestor(target2), wrapper);
  EXPECT_EQ(target1x1->CommonAncestor(target3), wrapper);

  EXPECT_EQ(target2->CommonAncestor(target1), wrapper);
  EXPECT_EQ(target2->CommonAncestor(target1x1), wrapper);
  EXPECT_EQ(target2->CommonAncestor(target2), target2);
  EXPECT_EQ(target2->CommonAncestor(target3), wrapper);

  EXPECT_EQ(target3->CommonAncestor(target1), wrapper);
  EXPECT_EQ(target3->CommonAncestor(target1x1), wrapper);
  EXPECT_EQ(target3->CommonAncestor(target2), wrapper);
  EXPECT_EQ(target3->CommonAncestor(target3), target3);
}

TEST_P(PaintLayerTest, PaintLayerCommonAncestorBody) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body, div {
        position: relative;
      }
    </style>
    <div id='target1'></div>
    <div id='target2'></div>
  )HTML");

  PaintLayer* target1 = GetPaintLayerByElementId("target1");
  PaintLayer* target2 = GetPaintLayerByElementId("target2");

  EXPECT_EQ(target1->CommonAncestor(target2)->GetLayoutObject(),
            GetDocument().body()->GetLayoutObject());
}

}  // namespace blink
