// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/compositing/CompositedLayerMapping.h"

#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "public/platform/WebContentLayer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef bool TestParamRootLayerScrolling;
class CompositedLayerMappingTest
    : public testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  CompositedLayerMappingTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  IntRect RecomputeInterestRect(const GraphicsLayer* graphics_layer) {
    return static_cast<CompositedLayerMapping*>(graphics_layer->Client())
        ->RecomputeInterestRect(graphics_layer);
  }

  IntRect ComputeInterestRect(
      const CompositedLayerMapping* composited_layer_mapping,
      GraphicsLayer* graphics_layer,
      IntRect previous_interest_rect) {
    return composited_layer_mapping->ComputeInterestRect(
        graphics_layer, previous_interest_rect);
  }

  bool ShouldFlattenTransform(const GraphicsLayer& layer) const {
    return layer.ShouldFlattenTransform();
  }

  bool InterestRectChangedEnoughToRepaint(const IntRect& previous_interest_rect,
                                          const IntRect& new_interest_rect,
                                          const IntSize& layer_size) {
    return CompositedLayerMapping::InterestRectChangedEnoughToRepaint(
        previous_interest_rect, new_interest_rect, layer_size);
  }

  IntRect PreviousInterestRect(const GraphicsLayer* graphics_layer) {
    return graphics_layer->previous_interest_rect_;
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  void TearDown() override { RenderingTest::TearDown(); }
};

#define EXPECT_RECT_EQ(expected, actual)                \
  do {                                                  \
    const IntRect& actual_rect = actual;                \
    EXPECT_EQ(expected.X(), actual_rect.X());           \
    EXPECT_EQ(expected.Y(), actual_rect.Y());           \
    EXPECT_EQ(expected.Width(), actual_rect.Width());   \
    EXPECT_EQ(expected.Height(), actual_rect.Height()); \
  } while (false)

INSTANTIATE_TEST_CASE_P(All, CompositedLayerMappingTest, ::testing::Bool());

TEST_P(CompositedLayerMappingTest, SubpixelAccumulationChange) {
  SetBodyInnerHTML(
      "<div id='target' style='will-change: transform; background: lightblue; "
      "position: relative; left: 0.4px; width: 100px; height: 100px'>");

  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyLeft, "0.6px");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // Directly composited layers are not invalidated on subpixel accumulation
  // change.
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking()
                   ->GetPaintController()
                   .GetPaintArtifact()
                   .IsEmpty());
}

TEST_P(CompositedLayerMappingTest,
       SubpixelAccumulationChangeIndirectCompositing) {
  SetBodyInnerHTML(
      "<div style='position; relative; width: 100px; height: 100px;"
      "    background: lightgray; will-change: transform'></div>"
      "<div id='target' style='background: lightblue; "
      "    position: relative; top: -10px; left: 0.4px; width: 100px;"
      "    height: 100px'></div>");

  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyLeft, "0.6px");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // The PaintArtifact should have been deleted because paint was
  // invalidated for subpixel accumulation change.
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking()
                   ->GetPaintController()
                   .GetPaintArtifact()
                   .IsEmpty());
}

TEST_P(CompositedLayerMappingTest, SimpleInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 200),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, TallLayerInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Screen-space visible content rect is [8, 8, 200, 600]. Mapping back to
  // local, adding 4000px in all directions, then clipping, yields this rect.
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 4592),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, TallLayerWholeDocumentInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform'></div>");

  GetDocument().GetSettings()->SetMainFrameClipsContent(false);

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  // Clipping is disabled in recomputeInterestRect.
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 10000),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
  EXPECT_RECT_EQ(
      IntRect(0, 0, 200, 10000),
      ComputeInterestRect(paint_layer->GetCompositedLayerMapping(),
                          paint_layer->GraphicsLayerBacking(), IntRect()));
}

TEST_P(CompositedLayerMappingTest, VerticalRightLeftWritingModeDocument) {
  SetBodyInnerHTML(
      "<style>html,body { margin: 0px } html { -webkit-writing-mode: "
      "vertical-rl}</style> <div id='target' style='width: 10000px; height: "
      "200px;'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(-5000, 0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  PaintLayer* paint_layer = GetDocument().GetLayoutViewItem().Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  // A scroll by -5000px is equivalent to a scroll by (10000 - 5000 - 800)px =
  // 4200px in non-RTL mode. Expanding the resulting rect by 4000px in each
  // direction yields this result.
  EXPECT_RECT_EQ(IntRect(200, 0, 8800, 600),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, RotatedInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform; transform: rotateZ(45deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 200),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, RotatedInterestRectNear90Degrees) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 10000px; height: 200px; will-change: "
      "transform; transform: rotateY(89.9999deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  // Because the layer is rotated to almost 90 degrees, floating-point error
  // leads to a reverse-projected rect that is much much larger than the
  // original layer size in certain dimensions. In such cases, we often fall
  // back to the 4000px interest rect padding amount.
  EXPECT_RECT_EQ(IntRect(0, 0, 4000, 200),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, 3D90DegRotatedTallInterestRect) {
  // It's rotated 90 degrees about the X axis, which means its visual content
  // rect is empty, and so the interest rect is the default (0, 0, 4000, 4000)
  // intersected with the layer bounds.
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateY(90deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 4000),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, 3D45DegRotatedTallInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateY(45deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 4592),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, RotatedTallInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateZ(45deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 4000),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, WideLayerInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 10000px; height: 200px; will-change: "
      "transform'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  // Screen-space visible content rect is [8, 8, 800, 200] (the screen is
  // 800x600).  Mapping back to local, adding 4000px in all directions, then
  // clipping, yields this rect.
  EXPECT_RECT_EQ(IntRect(0, 0, 4792, 200),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, FixedPositionInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 300px; height: 400px; will-change: "
      "transform; position: fixed; top: 100px; left: 200px;'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_RECT_EQ(IntRect(0, 0, 300, 400),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, LayerOffscreenInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform; position: absolute; top: 9000px; left: 0px;'>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 200),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, ScrollingLayerInterestRect) {
  SetBodyInnerHTML(
      "<style>div::-webkit-scrollbar{ width: 5px; }</style>"
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform; overflow: scroll'>"
      "<div style='width: 100px; height: 10000px'></div></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping()->ScrollingLayer());
  EXPECT_RECT_EQ(IntRect(0, 0, 195, 4592),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, ClippedBigLayer) {
  SetBodyInnerHTML(
      "<div style='width: 1px; height: 1px; overflow: hidden'>"
      "<div id='target' style='width: 10000px; height: 10000px; will-change: "
      "transform'></div></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  EXPECT_RECT_EQ(IntRect(0, 0, 4001, 4001),
                 RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, ClippingMaskLayer) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  const AtomicString style_without_clipping =
      "backface-visibility: hidden; width: 200px; height: 200px";
  const AtomicString style_with_border_radius =
      style_without_clipping + "; border-radius: 10px";
  const AtomicString style_with_clip_path =
      style_without_clipping + "; -webkit-clip-path: inset(10px)";

  SetBodyInnerHTML("<video id='video' src='x' style='" +
                   style_without_clipping + "'></video>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* video_element = GetDocument().getElementById("video");
  GraphicsLayer* graphics_layer =
      ToLayoutBoxModelObject(video_element->GetLayoutObject())
          ->Layer()
          ->GraphicsLayerBacking();
  EXPECT_FALSE(graphics_layer->MaskLayer());
  EXPECT_FALSE(graphics_layer->ContentsClippingMaskLayer());

  video_element->setAttribute(HTMLNames::styleAttr, style_with_border_radius);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(graphics_layer->MaskLayer());
  EXPECT_TRUE(graphics_layer->ContentsClippingMaskLayer());

  video_element->setAttribute(HTMLNames::styleAttr, style_with_clip_path);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(graphics_layer->MaskLayer());
  EXPECT_FALSE(graphics_layer->ContentsClippingMaskLayer());

  video_element->setAttribute(HTMLNames::styleAttr, style_without_clipping);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(graphics_layer->MaskLayer());
  EXPECT_FALSE(graphics_layer->ContentsClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest, ScrollContentsFlattenForScroller) {
  SetBodyInnerHTML(
      "<style>div::-webkit-scrollbar{ width: 5px; }</style>"
      "<div id='scroller' style='width: 100px; height: 100px; overflow: "
      "scroll; will-change: transform'>"
      "<div style='width: 1000px; height: 1000px;'>Foo</div>Foo</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  CompositedLayerMapping* composited_layer_mapping =
      paint_layer->GetCompositedLayerMapping();

  ASSERT_TRUE(composited_layer_mapping);

  EXPECT_FALSE(
      ShouldFlattenTransform(*composited_layer_mapping->MainGraphicsLayer()));
  EXPECT_FALSE(
      ShouldFlattenTransform(*composited_layer_mapping->ScrollingLayer()));
  EXPECT_TRUE(ShouldFlattenTransform(
      *composited_layer_mapping->ScrollingContentsLayer()));
}

TEST_P(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintEmpty) {
  IntSize layer_size(1000, 1000);
  // Both empty means there is nothing to do.
  EXPECT_FALSE(
      InterestRectChangedEnoughToRepaint(IntRect(), IntRect(), layer_size));
  // Going from empty to non-empty means we must re-record because it could be
  // the first frame after construction or Clear.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(IntRect(), IntRect(0, 0, 1, 1),
                                                 layer_size));
  // Going from non-empty to empty is not special-cased.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(IntRect(0, 0, 1, 1),
                                                  IntRect(), layer_size));
}

TEST_P(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintNotBigEnough) {
  IntSize layer_size(1000, 1000);
  IntRect previous_interest_rect(100, 100, 100, 100);
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 90, 90), layer_size));
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 100, 100), layer_size));
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(1, 1, 200, 200), layer_size));
}

TEST_P(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintNotBigEnoughButNewAreaTouchesEdge) {
  IntSize layer_size(500, 500);
  IntRect previous_interest_rect(100, 100, 100, 100);
  // Top edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 0, 100, 200), layer_size));
  // Left edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(0, 100, 200, 100), layer_size));
  // Bottom edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 100, 400), layer_size));
  // Right edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 400, 100), layer_size));
}

// Verifies that having a current viewport that touches a layer edge does not
// force re-recording.
TEST_P(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintCurrentViewportTouchesEdge) {
  IntSize layer_size(500, 500);
  IntRect new_interest_rect(100, 100, 300, 300);
  // Top edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(100, 0, 100, 100), new_interest_rect, layer_size));
  // Left edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(0, 100, 100, 100), new_interest_rect, layer_size));
  // Bottom edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(300, 400, 100, 100), new_interest_rect, layer_size));
  // Right edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(400, 300, 100, 100), new_interest_rect, layer_size));
}

TEST_P(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintScrollScenarios) {
  IntSize layer_size(1000, 1000);
  IntRect previous_interest_rect(100, 100, 100, 100);
  IntRect new_interest_rect(previous_interest_rect);
  new_interest_rect.Move(512, 0);
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
  new_interest_rect.Move(0, 512);
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
  new_interest_rect.Move(1, 0);
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
  new_interest_rect.Move(-1, 1);
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
}

TEST_P(CompositedLayerMappingTest, InterestRectChangeOnViewportScroll) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-scrollbar { width: 0; height: 0; }"
      "  body { margin: 0; }"
      "</style>"
      "<div id='div' style='width: 100px; height: 10000px'>Text</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutViewItem().Layer()->GraphicsLayerBacking();
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 4600),
                 PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 300), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because the recomputed rect hasn't
  // changed enough.
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 4900),
                 RecomputeInterestRect(root_scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 4600),
                 PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 600), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Use recomputed interest rect because it changed enough.
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 5200),
                 RecomputeInterestRect(root_scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 5200),
                 PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 5400), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_RECT_EQ(IntRect(0, 1400, 800, 8600),
                 RecomputeInterestRect(root_scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 1400, 800, 8600),
                 PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 9000), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because it contains the recomputed
  // interest rect.
  EXPECT_RECT_EQ(IntRect(0, 5000, 800, 5000),
                 RecomputeInterestRect(root_scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 1400, 800, 8600),
                 PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 2000), kProgrammaticScroll);
  // Use recomputed interest rect because it changed enough.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 6600),
                 RecomputeInterestRect(root_scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 6600),
                 PreviousInterestRect(root_scrolling_layer));
}

TEST_P(CompositedLayerMappingTest, InterestRectChangeOnShrunkenViewport) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-scrollbar { width: 0; height: 0; }"
      "  body { margin: 0; }"
      "</style>"
      "<div id='div' style='width: 100px; height: 10000px'>Text</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutViewItem().Layer()->GraphicsLayerBacking();
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 4600),
                 PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->SetFrameRect(IntRect(0, 0, 800, 60));
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Repaint required, so interest rect should be updated to shrunken size.
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 4060),
                 RecomputeInterestRect(root_scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 800, 4060),
                 PreviousInterestRect(root_scrolling_layer));
}

TEST_P(CompositedLayerMappingTest, InterestRectChangeOnScroll) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-scrollbar { width: 0; height: 0; }"
      "  body { margin: 0; }"
      "</style>"
      "<div id='scroller' style='width: 400px; height: 400px; overflow: "
      "scroll'>"
      "  <div id='content' style='width: 100px; height: 10000px'>Text</div>"
      "</div");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scroller = GetDocument().getElementById("scroller");
  GraphicsLayer* scrolling_layer =
      scroller->GetLayoutBox()->Layer()->GraphicsLayerBacking();
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 4600),
                 PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(300);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because the recomputed rect hasn't
  // changed enough.
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 4900),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 4600),
                 PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(600);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Use recomputed interest rect because it changed enough.
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 5200),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 5200),
                 PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(5400);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_RECT_EQ(IntRect(0, 1400, 400, 8600),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 1400, 400, 8600),
                 PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(9000);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because it contains the recomputed
  // interest rect.
  EXPECT_RECT_EQ(IntRect(0, 5000, 400, 5000),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 1400, 400, 8600),
                 PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(2000);
  // Use recomputed interest rect because it changed enough.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 6600),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 0, 400, 6600),
                 PreviousInterestRect(scrolling_layer));
}

TEST_P(CompositedLayerMappingTest,
       InterestRectShouldChangeOnPaintInvalidation) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-scrollbar { width: 0; height: 0; }"
      "  body { margin: 0; }"
      "</style>"
      "<div id='scroller' style='width: 400px; height: 400px; overflow: "
      "scroll'>"
      "  <div id='content' style='width: 100px; height: 10000px'>Text</div>"
      "</div");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scroller = GetDocument().getElementById("scroller");
  GraphicsLayer* scrolling_layer =
      scroller->GetLayoutBox()->Layer()->GraphicsLayerBacking();

  scroller->setScrollTop(5400);
  GetDocument().View()->UpdateAllLifecyclePhases();
  scroller->setScrollTop(9400);
  // The above code creates an interest rect bigger than the interest rect if
  // recomputed now.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_RECT_EQ(IntRect(0, 5400, 400, 4600),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 1400, 400, 8600),
                 PreviousInterestRect(scrolling_layer));

  // Paint invalidation and repaint should change previous paint interest rect.
  GetDocument().getElementById("content")->setTextContent("Change");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_RECT_EQ(IntRect(0, 5400, 400, 4600),
                 RecomputeInterestRect(scrolling_layer));
  EXPECT_RECT_EQ(IntRect(0, 5400, 400, 4600),
                 PreviousInterestRect(scrolling_layer));
}

TEST_P(CompositedLayerMappingTest,
       InterestRectOfSquashingLayerWithNegativeOverflow) {
  SetBodyInnerHTML(
      "<style>body { margin: 0; font-size: 16px; }</style>"
      "<div style='position: absolute; top: -500px; width: 200px; height: "
      "700px; will-change: transform'></div>"
      "<div id='squashed' style='position: absolute; top: 190px;'>"
      "  <div id='inside' style='width: 100px; height: 100px; text-indent: "
      "-10000px'>text</div>"
      "</div>");

  EXPECT_EQ(GetDocument()
                .getElementById("inside")
                ->GetLayoutBox()
                ->VisualOverflowRect()
                .Size()
                .Height(),
            100);

  CompositedLayerMapping* grouped_mapping = GetDocument()
                                                .getElementById("squashed")
                                                ->GetLayoutBox()
                                                ->Layer()
                                                ->GroupedMapping();
  // The squashing layer is at (-10000, 190, 10100, 100) in viewport
  // coordinates.
  // The following rect is at (-4000, 190, 4100, 100) in viewport coordinates.
  EXPECT_RECT_EQ(IntRect(6000, 0, 4100, 100),
                 grouped_mapping->ComputeInterestRect(
                     grouped_mapping->SquashingLayer(), IntRect()));
}

TEST_P(CompositedLayerMappingTest,
       InterestRectOfSquashingLayerWithAncestorClip) {
  SetBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div style='overflow: hidden; width: 400px; height: 400px'>"
      "  <div style='position: relative; backface-visibility: hidden'>"
      "    <div style='position: absolute; top: -500px; width: 200px; height: "
      "700px; backface-visibility: hidden'></div>"
      // Above overflow:hidden div and two composited layers make the squashing
      // layer a child of an ancestor clipping layer.
      "    <div id='squashed' style='height: 1000px; width: 10000px; right: 0; "
      "position: absolute'></div>"
      "  </div>"
      "</div>");

  CompositedLayerMapping* grouped_mapping = GetDocument()
                                                .getElementById("squashed")
                                                ->GetLayoutBox()
                                                ->Layer()
                                                ->GroupedMapping();
  // The squashing layer is at (-9600, 0, 10000, 1000) in viewport coordinates.
  // The following rect is at (-4000, 0, 4400, 1000) in viewport coordinates.
  EXPECT_RECT_EQ(IntRect(5600, 0, 4400, 1000),
                 grouped_mapping->ComputeInterestRect(
                     grouped_mapping->SquashingLayer(), IntRect()));
}

TEST_P(CompositedLayerMappingTest, InterestRectOfIframeInScrolledDiv) {
  GetDocument().SetBaseURLOverride(KURL(kParsedURLString, "http://test.com"));
  SetBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div style='width: 200; height: 8000px'></div>"
      "<iframe src='http://test.com' width='500' height='500' "
      "frameBorder='0'>"
      "</iframe>");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; height: 200px; "
      "will-change: transform}</style><div id=target></div>");

  // Scroll 8000 pixels down to move the iframe into view.
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0.0, 8000.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* target = ChildDocument().getElementById("target");
  ASSERT_TRUE(target);

  EXPECT_RECT_EQ(
      IntRect(0, 0, 200, 200),
      RecomputeInterestRect(
          target->GetLayoutObject()->EnclosingLayer()->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, InterestRectOfScrolledIframe) {
  GetDocument().SetBaseURLOverride(KURL(kParsedURLString, "http://test.com"));
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(
      "<style>body { margin: 0; } ::-webkit-scrollbar { display: none; "
      "}</style>"
      "<iframe src='http://test.com' width='500' height='500' "
      "frameBorder='0'>"
      "</iframe>");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; "
      "height: 8000px;}</style><div id=target></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Scroll 7500 pixels down to bring the scrollable area to the bottom.
  ChildDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0.0, 7500.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(ChildDocument().View()->GetLayoutViewItem().HasLayer());
  EXPECT_RECT_EQ(IntRect(0, 3500, 500, 4500),
                 RecomputeInterestRect(ChildDocument()
                                           .View()
                                           ->GetLayoutViewItem()
                                           .EnclosingLayer()
                                           ->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest, InterestRectOfIframeWithContentBoxOffset) {
  GetDocument().SetBaseURLOverride(KURL(kParsedURLString, "http://test.com"));
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  // Set a 10px border in order to have a contentBoxOffset for the iframe
  // element.
  SetBodyInnerHTML(
      "<style>body { margin: 0; } #frame { border: 10px solid black; } "
      "::-webkit-scrollbar { display: none; }</style>"
      "<iframe src='http://test.com' width='500' height='500' "
      "frameBorder='0'>"
      "</iframe>");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; "
      "height: 8000px;}</style> <div id=target></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Scroll 3000 pixels down to bring the scrollable area to somewhere in the
  // middle.
  ChildDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0.0, 3000.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(ChildDocument().View()->GetLayoutViewItem().HasLayer());
  // The width is 485 pixels due to the size of the scrollbar.
  EXPECT_RECT_EQ(IntRect(0, 0, 500, 7500),
                 RecomputeInterestRect(ChildDocument()
                                           .View()
                                           ->GetLayoutViewItem()
                                           .EnclosingLayer()
                                           ->GraphicsLayerBacking()));
}

TEST_P(CompositedLayerMappingTest,
       ScrollingContentsAndForegroundLayerPaintingPhase) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(
      "<div id='container' style='position: relative; z-index: 1; overflow: "
      "scroll; width: 300px; height: 300px'>"
      "    <div id='negative-composited-child' style='background-color: red; "
      "width: 1px; height: 1px; position: absolute; backface-visibility: "
      "hidden; z-index: -1'></div>"
      "    <div style='background-color: blue; width: 2000px; height: 2000px; "
      "position: relative; top: 10px'></div>"
      "</div>");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("container"))
          ->Layer()
          ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping->ScrollingContentsLayer());
  EXPECT_EQ(static_cast<GraphicsLayerPaintingPhase>(
                kGraphicsLayerPaintOverflowContents |
                kGraphicsLayerPaintCompositedScroll),
            mapping->ScrollingContentsLayer()->PaintingPhase());
  ASSERT_TRUE(mapping->ForegroundLayer());
  EXPECT_EQ(
      static_cast<GraphicsLayerPaintingPhase>(
          kGraphicsLayerPaintForeground | kGraphicsLayerPaintOverflowContents),
      mapping->ForegroundLayer()->PaintingPhase());

  Element* negative_composited_child =
      GetDocument().getElementById("negative-composited-child");
  negative_composited_child->parentNode()->RemoveChild(
      negative_composited_child);
  GetDocument().View()->UpdateAllLifecyclePhases();

  mapping = ToLayoutBlock(GetLayoutObjectByElementId("container"))
                ->Layer()
                ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping->ScrollingContentsLayer());
  EXPECT_EQ(
      static_cast<GraphicsLayerPaintingPhase>(
          kGraphicsLayerPaintOverflowContents |
          kGraphicsLayerPaintCompositedScroll | kGraphicsLayerPaintForeground),
      mapping->ScrollingContentsLayer()->PaintingPhase());
  EXPECT_FALSE(mapping->ForegroundLayer());
}

TEST_P(CompositedLayerMappingTest,
       DecorationOutlineLayerOnlyCreatedInCompositedScrolling) {
  SetBodyInnerHTML(
      "<style>"
      "#target { overflow: scroll; height: 200px; width: 200px; will-change: "
      "transform; background: white local content-box; "
      "outline: 1px solid blue; outline-offset: -2px;}"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"target\"><div id=\"scrolled\"></div></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  // Decoration outline layer is created when composited scrolling.
  EXPECT_TRUE(paint_layer->HasCompositedLayerMapping());
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  CompositedLayerMapping* mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(mapping->DecorationOutlineLayer());

  // No decoration outline layer is created when not composited scrolling.
  element->setAttribute(HTMLNames::styleAttr, "overflow: visible;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(mapping->DecorationOutlineLayer());
}

TEST_P(CompositedLayerMappingTest,
       DecorationOutlineLayerCreatedAndDestroyedInCompositedScrolling) {
  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; outline: 1px solid blue; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"scroller\"><div id=\"scrolled\"></div></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  CompositedLayerMapping* mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->DecorationOutlineLayer());

  // The decoration outline layer is created when composited scrolling
  // with an outline drawn over the composited scrolling region.
  scroller->setAttribute(HTMLNames::styleAttr, "outline-offset: -2px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(mapping->DecorationOutlineLayer());

  // The decoration outline layer is destroyed when the scrolling region
  // will not be covered up by the outline.
  scroller->removeAttribute(HTMLNames::styleAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->DecorationOutlineLayer());
}

TEST_P(CompositedLayerMappingTest,
       BackgroundPaintedIntoGraphicsLayerIfNotCompositedScrolling) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(
      "<div id='container' style='overflow: scroll; width: 300px; height: "
      "300px; border-radius: 5px; background: white; will-change: transform;'>"
      "    <div style='background-color: blue; width: 2000px; height: "
      "2000px;'></div>"
      "</div>");

  PaintLayer* layer =
      ToLayoutBlock(GetLayoutObjectByElementId("container"))->Layer();
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            layer->GetBackgroundPaintLocation());

  // We currently don't use composited scrolling when the container has a
  // border-radius so even though we can paint the background onto the scrolling
  // contents layer we don't have a scrolling contents layer to paint into in
  // this case.
  CompositedLayerMapping* mapping = layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->HasScrollingLayer());
  EXPECT_FALSE(mapping->BackgroundPaintsOntoScrollingContentsLayer());
}

TEST_P(CompositedLayerMappingTest,
       ScrollingLayerWithPerspectivePositionedCorrectly) {
  // Test positioning of a scrolling layer within an offset parent, both with
  // and without perspective.
  //
  // When a box shadow is used, the main graphics layer position is offset by
  // the shadow. The scrolling contents then need to be offset in the other
  // direction to compensate.  To make this a little clearer, for the first
  // example here the layer positions are calculated as:
  //
  //   m_graphicsLayer x = left_pos - shadow_spread + shadow_x_offset
  //                     = 50 - 10 - 10
  //                     = 30
  //
  //   m_graphicsLayer y = top_pos - shadow_spread + shadow_y_offset
  //                     = 50 - 10 + 0
  //                     = 40
  //
  //   contents x = 50 - m_graphicsLayer x = 50 - 30 = 20
  //   contents y = 50 - m_graphicsLayer y = 50 - 40 = 10
  //
  // The reason that perspective matters is that it affects which 'contents'
  // layer is offset; m_childTransformLayer when using perspective, or
  // m_scrollingLayer when there is no perspective.

  SetBodyInnerHTML(
      "<div id='scroller' style='position: absolute; top: 50px; left: 50px; "
      "width: 400px; height: 245px; overflow: auto; will-change: transform; "
      "box-shadow: -10px 0 0 10px; perspective: 1px;'>"
      "    <div style='position: absolute; top: 50px; bottom: 0; width: 200px; "
      "height: 200px;'></div>"
      "</div>"

      "<div id='scroller2' style='position: absolute; top: 400px; left: 50px; "
      "width: 400px; height: 245px; overflow: auto; will-change: transform; "
      "box-shadow: -10px 0 0 10px;'>"
      "    <div style='position: absolute; top: 50px; bottom: 0; width: 200px; "
      "height: 200px;'></div>"
      "</div>");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))
          ->Layer()
          ->GetCompositedLayerMapping();

  CompositedLayerMapping* mapping2 =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller2"))
          ->Layer()
          ->GetCompositedLayerMapping();

  ASSERT_TRUE(mapping);
  ASSERT_TRUE(mapping2);

  // The perspective scroller should have a child transform containing the
  // positional offset, and a scrolling layer that has no offset.

  GraphicsLayer* scrolling_layer = mapping->ScrollingLayer();
  GraphicsLayer* child_transform_layer = mapping->ChildTransformLayer();
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  ASSERT_TRUE(scrolling_layer);
  ASSERT_TRUE(child_transform_layer);

  EXPECT_FLOAT_EQ(30, main_graphics_layer->GetPosition().X());
  EXPECT_FLOAT_EQ(40, main_graphics_layer->GetPosition().Y());
  EXPECT_FLOAT_EQ(0, scrolling_layer->GetPosition().X());
  EXPECT_FLOAT_EQ(0, scrolling_layer->GetPosition().Y());
  EXPECT_FLOAT_EQ(20, child_transform_layer->GetPosition().X());
  EXPECT_FLOAT_EQ(10, child_transform_layer->GetPosition().Y());

  // The non-perspective scroller should have no child transform and the
  // offset on the scroller layer directly.

  GraphicsLayer* scrolling_layer2 = mapping2->ScrollingLayer();
  GraphicsLayer* main_graphics_layer2 = mapping2->MainGraphicsLayer();

  ASSERT_TRUE(scrolling_layer2);
  ASSERT_FALSE(mapping2->ChildTransformLayer());

  EXPECT_FLOAT_EQ(30, main_graphics_layer2->GetPosition().X());
  EXPECT_FLOAT_EQ(390, main_graphics_layer2->GetPosition().Y());
  EXPECT_FLOAT_EQ(20, scrolling_layer2->GetPosition().X());
  EXPECT_FLOAT_EQ(10, scrolling_layer2->GetPosition().Y());
}

TEST_P(CompositedLayerMappingTest, AncestorClippingMaskLayerUpdates) {
  SetBodyInnerHTML(
      "<style>"
      "  #ancestor { width: 100px; height: 100px; overflow: hidden; }"
      "  #child { width: 120px; height: 120px; background-color: green; }"
      "</style>"
      "<div id='ancestor'>"
      "  <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_FALSE(child_paint_layer);

  // Making the child conposited causes creation of an AncestorClippingLayer.
  child->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());

  // Adding border radius to the ancestor requires an
  // ancestorClippingMaskLayer for the child
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());

  // Removing the border radius should remove the ancestorClippingMaskLayer
  // for the child
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 0px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());

  // Add border radius back so we can test one more case
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Now change the overflow to remove the need for an ancestor clip
  // on the child
  ancestor->setAttribute(HTMLNames::styleAttr, "overflow: visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_FALSE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest, AncestorClippingMaskLayerSiblingUpdates) {
  SetBodyInnerHTML(
      "<style>"
      "  #ancestor { width: 200px; height: 200px; overflow: hidden; }"
      "  #child1 { width: 10px;; height: 260px; position: relative; "
      "            left: 0px; top: -30px; background-color: green; }"
      "  #child2 { width: 10px;; height: 260px; position: relative; "
      "            left: 190px; top: -260px; background-color: green; }"
      "</style>"
      "<div id='ancestor'>"
      "  <div id='child1'></div>"
      "  <div id='child2'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child1 = GetDocument().getElementById("child1");
  ASSERT_TRUE(child1);
  PaintLayer* child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  CompositedLayerMapping* child1_mapping =
      child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child1_mapping);

  Element* child2 = GetDocument().getElementById("child2");
  ASSERT_TRUE(child2);
  PaintLayer* child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  CompositedLayerMapping* child2_mapping =
      child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child2_mapping);

  // Making child1 composited causes creation of an AncestorClippingLayer.
  child1->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child1_mapping);
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child1_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child1_mapping->AncestorClippingMaskLayer());
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child2_mapping);

  // Adding border radius to the ancestor requires an
  // ancestorClippingMaskLayer for child1
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child1_mapping);
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingMaskLayer());
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child2_mapping);

  // Making child2 composited causes creation of an AncestorClippingLayer
  // and a mask layer.
  child2->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child1_mapping);
  ASSERT_TRUE(child1_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingMaskLayer());
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child2_mapping);
  ASSERT_TRUE(child2_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingMaskLayer());

  // Removing will-change: transform on child1 should result in the removal
  // of all clipping and masking layers
  child1->setAttribute(HTMLNames::styleAttr, "will-change: none");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(child1_mapping);
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child2_mapping);
  ASSERT_TRUE(child2_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingMaskLayer());

  // Now change the overflow to remove the need for an ancestor clip
  // on the children
  ancestor->setAttribute(HTMLNames::styleAttr, "overflow: visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(child1_mapping);
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child2_mapping);
  EXPECT_FALSE(child2_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child2_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest, AncestorClippingMaskLayerGrandchildUpdates) {
  SetBodyInnerHTML(
      "<style>"
      "  #ancestor { width: 200px; height: 200px; overflow: hidden; }"
      "  #child { width: 10px;; height: 260px; position: relative; "
      "           left: 0px; top: -30px; background-color: green; }"
      "  #grandchild { width: 10px;; height: 260px; position: relative; "
      "                left: 190px; top: -30px; background-color: green; }"
      "</style>"
      "<div id='ancestor'>"
      "  <div id='child'>"
      "    <div id='grandchild'></div>"
      "  </div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);

  Element* grandchild = GetDocument().getElementById("grandchild");
  ASSERT_TRUE(grandchild);
  PaintLayer* grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  CompositedLayerMapping* grandchild_mapping =
      grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(grandchild_mapping);

  // Making grandchild composited causes creation of an AncestorClippingLayer.
  grandchild->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  EXPECT_TRUE(grandchild_mapping->AncestorClippingLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingMaskLayer());

  // Adding border radius to the ancestor requires an
  // ancestorClippingMaskLayer for grandchild
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  ASSERT_TRUE(grandchild_mapping->AncestorClippingLayer());
  EXPECT_TRUE(grandchild_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(grandchild_mapping->AncestorClippingMaskLayer());

  // Moving the grandchild out of the clip region should result in removal
  // of the mask layer. It also removes the grandchild from its own mapping
  // because it is now squashed.
  grandchild->setAttribute(HTMLNames::styleAttr,
                           "left: 250px; will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  ASSERT_TRUE(grandchild_mapping->AncestorClippingLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingMaskLayer());

  // Now change the overflow to remove the need for an ancestor clip
  // on the children
  ancestor->setAttribute(HTMLNames::styleAttr, "overflow: visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  EXPECT_FALSE(grandchild_mapping->AncestorClippingLayer());
}

TEST_P(CompositedLayerMappingTest, AncestorClipMaskRequiredByBorderRadius) {
  // Verify that we create the mask layer when the child is contained within
  // the rectangular clip but not contained within the rounded rect clip.
  SetBodyInnerHTML(
      "<style>"
      "  #ancestor {"
      "    width: 100px; height: 100px; overflow: hidden; border-radius: 20px;"
      "  }"
      "  #child { position: relative; left: 2px; top: 2px; width: 96px;"
      "           height: 96px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='ancestor'>"
      "  <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskNotRequiredByNestedBorderRadius) {
  // This case has the child within all ancestors and does not require a
  // mask.
  SetBodyInnerHTML(
      "<style>"
      "  #grandparent {"
      "    width: 200px; height: 200px; overflow: hidden; border-radius: 25px;"
      "  }"
      "  #parent { position: relative; left: 40px; top: 40px; width: 120px;"
      "           height: 120px; border-radius: 10px; overflow: hidden;"
      "  }"
      "  #child { position: relative; left: 10px; top: 10px; width: 100px;"
      "           height: 100px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='grandparent'>"
      "  <div id='parent'>"
      "    <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskRequiredByParentBorderRadius) {
  // This case has the child within the grandparent but not the parent, and does
  // require a mask so that the parent will clip the corners.
  SetBodyInnerHTML(
      "<style>"
      "  #grandparent {"
      "    width: 200px; height: 200px; overflow: hidden; border-radius: 25px;"
      "  }"
      "  #parent { position: relative; left: 40px; top: 40px; width: 120px;"
      "           height: 120px; border-radius: 10px; overflow: hidden;"
      "  }"
      "  #child { position: relative; left: 1px; top: 1px; width: 118px;"
      "           height: 118px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='grandparent'>"
      "  <div id='parent'>"
      "    <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
  const FloatSize layer_size =
      child_mapping->AncestorClippingMaskLayer()->Size();
  EXPECT_EQ(120, layer_size.Width());
  EXPECT_EQ(120, layer_size.Height());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskNotRequiredByParentBorderRadius) {
  // This case has the child within the grandparent but not the parent, and does
  // not require a mask because the parent does not have border radius
  SetBodyInnerHTML(
      "<style>"
      "  #grandparent {"
      "    width: 200px; height: 200px; overflow: hidden; border-radius: 25px;"
      "  }"
      "  #parent { position: relative; left: 40px; top: 40px; width: 120px;"
      "           height: 120px; overflow: hidden;"
      "  }"
      "  #child { position: relative; left: -10px; top: -10px; width: 140px;"
      "           height: 140px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='grandparent'>"
      "  <div id='parent'>"
      "    <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskRequiredByGrandparentBorderRadius1) {
  // This case has the child clipped by the grandparent border radius but not
  // the parent, and requires a mask to clip to the grandparent. Although in
  // an optimized world we would not need this because the parent clips out
  // the child before it is clipped by the grandparent.
  SetBodyInnerHTML(
      "<style>"
      "  #grandparent {"
      "    width: 200px; height: 200px; overflow: hidden; border-radius: 25px;"
      "  }"
      "  #parent { position: relative; left: 40px; top: 40px; width: 120px;"
      "           height: 120px; overflow: hidden;"
      "  }"
      "  #child { position: relative; left: -10px; top: -10px; width: 180px;"
      "           height: 180px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='grandparent'>"
      "  <div id='parent'>"
      "    <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
  const FloatSize layer_size =
      child_mapping->AncestorClippingMaskLayer()->Size();
  EXPECT_EQ(120, layer_size.Width());
  EXPECT_EQ(120, layer_size.Height());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskRequiredByGrandparentBorderRadius2) {
  // Similar to the previous case, but here we really do need the mask.
  SetBodyInnerHTML(
      "<style>"
      "  #grandparent {"
      "    width: 200px; height: 200px; overflow: hidden; border-radius: 25px;"
      "  }"
      "  #parent { position: relative; left: 40px; top: 40px; width: 180px;"
      "           height: 180px; overflow: hidden;"
      "  }"
      "  #child { position: relative; left: -10px; top: -10px; width: 180px;"
      "           height: 180px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='grandparent'>"
      "  <div id='parent'>"
      "    <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
  const FloatSize layer_size =
      child_mapping->AncestorClippingMaskLayer()->Size();
  EXPECT_EQ(160, layer_size.Width());
  EXPECT_EQ(160, layer_size.Height());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskNotRequiredByBorderRadiusInside) {
  // Verify that we do not create the mask layer when the child is contained
  // within the rounded rect clip.
  SetBodyInnerHTML(
      "<style>"
      "  #ancestor {"
      "    width: 100px; height: 100px; overflow: hidden; border-radius: 5px;"
      "  }"
      "  #child { position: relative; left: 10px; top: 10px; width: 80px;"
      "           height: 80px; background-color: green;"
      "           will-change: transform;"
      "  }"
      "</style>"
      "<div id='ancestor'>"
      "  <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest,
       AncestorClipMaskNotRequiredByBorderRadiusOutside) {
  // Verify that we do not create the mask layer when the child is outside
  // the ancestors rectangular clip.
  SetBodyInnerHTML(
      "<style>"
      "  #ancestor {"
      "    width: 100px; height: 100px; overflow: hidden; border-radius: 5px;"
      "  }"
      "  #child { position: relative; left: 110px; top: 10px; width: 80px;"
      "           height: 80px; background-color: green;"
      "           will-change: transform;"
      "}"
      "</style>"
      "<div id='ancestor'>"
      "  <div id='child'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_P(CompositedLayerMappingTest, StickyPositionMainThreadOffset) {
  SetBodyInnerHTML(
      "<style>.composited { backface-visibility: hidden; }"
      "#scroller { overflow: auto; height: 200px; width: 200px; }"
      ".container { height: 500px; }"
      ".innerPadding { height: 10px; }"
      "#sticky { position: sticky; top: 25px; height: 50px; }</style>"
      "<div id='scroller' class='composited'>"
      "  <div class='composited container'>"
      "    <div class='composited container'>"
      "      <div class='innerPadding'></div>"
      "      <div id='sticky' class='composited'></div>"
      "  </div></div></div>");

  PaintLayer* sticky_layer =
      ToLayoutBox(GetLayoutObjectByElementId("sticky"))->Layer();
  CompositedLayerMapping* sticky_mapping =
      sticky_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(sticky_mapping);

  // Now scroll the page - this should increase the main thread offset.
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  sticky_layer->SetNeedsCompositingInputsUpdate();
  EXPECT_TRUE(sticky_layer->NeedsCompositingInputsUpdate());
  GetDocument().View()->UpdateLifecycleToCompositingCleanPlusScrolling();
  EXPECT_FALSE(sticky_layer->NeedsCompositingInputsUpdate());
}

TEST_P(CompositedLayerMappingTest, StickyPositionNotSquashed) {
  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: auto; height: 200px; }"
      "#sticky1, #sticky2, #sticky3 {position: sticky; top: 0; width: 50px;"
      "    height: 50px; background: rgba(0, 128, 0, 0.5);}"
      "#sticky1 {backface-visibility: hidden;}"
      ".spacer {height: 2000px;}"
      "</style>"
      "<div id='scroller'>"
      "  <div id='sticky1'></div>"
      "  <div id='sticky2'></div>"
      "  <div id='sticky3'></div>"
      "  <div class='spacer'></div>"
      "</div>");

  PaintLayer* sticky1 =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky1"))->Layer();
  PaintLayer* sticky2 =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky2"))->Layer();
  PaintLayer* sticky3 =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky3"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky1->GetCompositingState());
  EXPECT_EQ(kNotComposited, sticky2->GetCompositingState());
  EXPECT_EQ(kNotComposited, sticky3->GetCompositingState());

  PaintLayer* scroller =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Now that sticky2 and sticky3 overlap sticky1 they will be promoted, but
  // they should not be squashed into the same layer because they scroll with
  // respect to each other.
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky1->GetCompositingState());
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky2->GetCompositingState());
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky3->GetCompositingState());
}

TEST_P(CompositedLayerMappingTest,
       LayerPositionForStickyElementInCompositedScroller) {
  SetBodyInnerHTML(
      "<style>"
      " .scroller { overflow: scroll; width: 200px; height: 600px; }"
      " .composited { will-change:transform; }"
      " .box { position: sticky; width: 185px; height: 50px; top: 0px; }"
      " .container { width: 100%; height: 1000px; }"
      "</style>"
      "<div id='scroller' class='composited scroller'>"
      " <div class='composited container'>"
      "  <div id='sticky' class='box'></div>"
      " </div>"
      "</div>");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky"))
          ->Layer()
          ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  PaintLayer* scroller =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(0, main_graphics_layer->GetPosition().X());
  EXPECT_FLOAT_EQ(0, main_graphics_layer->GetPosition().Y());
}

TEST_P(CompositedLayerMappingTest,
       LayerPositionForStickyElementInNonCompositedScroller) {
  SetBodyInnerHTML(
      "<style>"
      " .scroller { overflow: scroll; width: 200px; height: 600px; }"
      " .composited { will-change:transform; }"
      " .box { position: sticky; width: 185px; height: 50px; top: 0px; }"
      " .container { width: 100%; height: 1000px; }"
      "</style>"
      "<div id='scroller' class='scroller'>"
      " <div class='composited container'>"
      "  <div id='sticky' class='box'></div>"
      " </div>"
      "</div>");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky"))
          ->Layer()
          ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  PaintLayer* scroller =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(0, main_graphics_layer->GetPosition().X());
  EXPECT_FLOAT_EQ(100, main_graphics_layer->GetPosition().Y());
}

TEST_P(CompositedLayerMappingTest,
       TransformedRasterizationDisallowedForDirectReasons) {
  // This test verifies layers with direct compositing reasons won't have
  // transformed rasterization, i.e. should raster in local space.
  SetBodyInnerHTML(
      "<div id='target1' style='transform:translateZ(0);'>foo</div>"
      "<div id='target2' style='will-change:opacity;'>bar</div>"
      "<div id='target3' style='backface-visibility:hidden;'>ham</div>");

  {
    LayoutObject* target = GetLayoutObjectByElementId("target1");
    ASSERT_TRUE(target && target->IsBox());
    PaintLayer* target_layer = ToLayoutBox(target)->Layer();
    GraphicsLayer* target_graphics_layer =
        target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
    ASSERT_TRUE(target_graphics_layer);
    EXPECT_FALSE(target_graphics_layer->ContentLayer()
                     ->TransformedRasterizationAllowed());
  }
  {
    LayoutObject* target = GetLayoutObjectByElementId("target2");
    ASSERT_TRUE(target && target->IsBox());
    PaintLayer* target_layer = ToLayoutBox(target)->Layer();
    GraphicsLayer* target_graphics_layer =
        target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
    ASSERT_TRUE(target_graphics_layer);
    EXPECT_FALSE(target_graphics_layer->ContentLayer()
                     ->TransformedRasterizationAllowed());
  }
  {
    LayoutObject* target = GetLayoutObjectByElementId("target3");
    ASSERT_TRUE(target && target->IsBox());
    PaintLayer* target_layer = ToLayoutBox(target)->Layer();
    GraphicsLayer* target_graphics_layer =
        target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
    ASSERT_TRUE(target_graphics_layer);
    EXPECT_FALSE(target_graphics_layer->ContentLayer()
                     ->TransformedRasterizationAllowed());
  }
}

TEST_P(CompositedLayerMappingTest, TransformedRasterizationForInlineTransform) {
  // This test verifies we allow layers that are indirectly composited due to
  // an inline transform (but no direct reason otherwise) to raster in the
  // device space for higher quality.
  SetBodyInnerHTML(
      "<div style='will-change:transform; width:500px; "
      "height:20px;'>composited</div>"
      "<div id='target' style='transform:translate(1.5px,-10.5px); "
      "width:500px; height:20px;'>indirectly composited due to inline "
      "transform</div>");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(target && target->IsBox());
  PaintLayer* target_layer = ToLayoutBox(target)->Layer();
  GraphicsLayer* target_graphics_layer =
      target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
  ASSERT_TRUE(target_graphics_layer);
  EXPECT_TRUE(
      target_graphics_layer->ContentLayer()->TransformedRasterizationAllowed());
}

}  // namespace blink
