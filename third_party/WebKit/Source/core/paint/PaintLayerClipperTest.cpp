// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerClipper.h"

#include "build/build_config.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/LayoutTestSupport.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"

namespace blink {

class PaintLayerClipperTest : public RenderingTest {
 public:
  PaintLayerClipperTest() : RenderingTest(EmptyLocalFrameClient::Create()) {}

  void SetUp() override {
    LayoutTestSupport::SetMockThemeEnabledForTest(true);
    RenderingTest::SetUp();
  }

  void TearDown() override {
    LayoutTestSupport::SetMockThemeEnabledForTest(false);
    RenderingTest::TearDown();
  }
};

TEST_F(PaintLayerClipperTest, BackgroundClipRectSubpixelAccumulation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <svg id=target width=200 height=300 style='position: relative'>
      <rect width=400 height=500 fill='blue'/>
    </svg>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, LayoutSize(FloatSize(0.25, 0.35)));
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    context.SetIgnoreOverflowClip();
  ClipRect background_rect;

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect);

  EXPECT_EQ(LayoutRect(FloatRect(8.25, 8.35, 200, 300)),
            background_rect.Rect());
}

TEST_F(PaintLayerClipperTest, LayoutSVGRoot) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <svg id=target width=200 height=300 style='position: relative'>
      <rect width=400 height=500 fill='blue'/>
    </svg>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, LayoutSize(FloatSize(0.25, 0.35)));
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    context.SetIgnoreOverflowClip();
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);

  EXPECT_EQ(LayoutRect(FloatRect(8.25, 8.35, 200, 300)),
            background_rect.Rect());
  EXPECT_EQ(LayoutRect(FloatRect(8.25, 8.35, 200, 300)),
            foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(FloatRect(8.25, 8.35, 200, 300)), layer_bounds);
}

TEST_F(PaintLayerClipperTest, ControlClip) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <input id=target style='position:absolute; width: 200px; height: 300px'
        type=button>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           kUncachedClipRects);
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    context.SetIgnoreOverflowClip();
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);
#if defined(OS_MACOSX)
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(LayoutRect(3, 4, 210, 28), background_rect.Rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(LayoutRect(8, 8, 200, 18), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 18), layer_bounds);
#else
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), background_rect.Rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(LayoutRect(10, 10, 196, 296), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), layer_bounds);
#endif
}

TEST_F(PaintLayerClipperTest, RoundedClip) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='target' style='position:absolute; width: 200px; height: 300px;
        overflow: hidden; border-radius: 1px'>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           kUncachedClipRects);
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    context.SetIgnoreOverflowClip();

  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);

  // Only the foreground rect gets hasRadius set for overflow clipping
  // of descendants.
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), background_rect.Rect());
  EXPECT_FALSE(background_rect.HasRadius());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), foreground_rect.Rect());
  EXPECT_TRUE(foreground_rect.HasRadius());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), layer_bounds);
}

TEST_F(PaintLayerClipperTest, RoundedClipNested) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='parent' style='position:absolute; width: 200px; height: 300px;
        overflow: hidden; border-radius: 1px'>
      <div id='child' style='position: relative; width: 500px;
           height: 500px'>
      </div>
    </div>
  )HTML");

  Element* parent = GetDocument().getElementById("parent");
  PaintLayer* parent_paint_layer =
      ToLayoutBoxModelObject(parent->GetLayoutObject())->Layer();

  Element* child = GetDocument().getElementById("child");
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();

  ClipRectsContext context(parent_paint_layer, kUncachedClipRects);

  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  child_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &child_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);

  EXPECT_EQ(LayoutRect(0, 0, 200, 300), background_rect.Rect());
  EXPECT_TRUE(background_rect.HasRadius());
  EXPECT_EQ(LayoutRect(0, 0, 200, 300), foreground_rect.Rect());
  EXPECT_TRUE(foreground_rect.HasRadius());
  EXPECT_EQ(LayoutRect(0, 0, 500, 500), layer_bounds);
}

TEST_F(PaintLayerClipperTest, ControlClipSelect) {
  SetBodyInnerHTML(R"HTML(
    <select id='target' style='position: relative; width: 100px;
        background: none; border: none; padding: 0px 15px 0px 5px;'>
      <option>
        Test long texttttttttttttttttttttttttttttttt
      </option>
    </select>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           kUncachedClipRects);
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    context.SetIgnoreOverflowClip();
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);
// The control clip for a select excludes the area for the down arrow.
#if defined(OS_MACOSX)
  EXPECT_EQ(LayoutRect(16, 9, 79, 13), foreground_rect.Rect());
#elif defined(OS_WIN)
  EXPECT_EQ(LayoutRect(17, 9, 60, 16), foreground_rect.Rect());
#else
  EXPECT_EQ(LayoutRect(17, 9, 60, 15), foreground_rect.Rect());
#endif
}

TEST_F(PaintLayerClipperTest, LayoutSVGRootChild) {
  SetBodyInnerHTML(R"HTML(
    <svg width=200 height=300 style='position: relative'>
      <foreignObject width=400 height=500>
        <div id=target xmlns='http://www.w3.org/1999/xhtml'
    style='position: relative'></div>
      </foreignObject>
    </svg>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           kUncachedClipRects);
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), background_rect.Rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(8, 8, 400, 0), layer_bounds);
}

TEST_F(PaintLayerClipperTest, ContainPaintClip) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
        style='contain: paint; width: 200px; height: 200px; overflow: auto'>
      <div style='height: 400px'></div>
    </div>
  )HTML");

  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  PaintLayer* layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(layer, kPaintingClipRectsIgnoringOverflowClip);
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &layer->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);
  EXPECT_GE(background_rect.Rect().Size().Width().ToInt(), 33554422);
  EXPECT_GE(background_rect.Rect().Size().Height().ToInt(), 33554422);
  EXPECT_EQ(background_rect.Rect(), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), layer_bounds);

  ClipRectsContext context_clip(layer, kUncachedClipRects);

  layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context_clip, &layer->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), background_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), layer_bounds);
}

TEST_F(PaintLayerClipperTest, NestedContainPaintClip) {
  SetBodyInnerHTML(R"HTML(
    <div style='contain: paint; width: 200px; height: 200px; overflow:
    auto'>
      <div id='target' style='contain: paint; height: 400px'>
      </div>
    </div>
  )HTML");

  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  PaintLayer* layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(layer->Parent(),
                           kPaintingClipRectsIgnoringOverflowClip);
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &layer->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), background_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), layer_bounds);

  ClipRectsContext context_clip(layer->Parent(), kUncachedClipRects);

  layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context_clip, &layer->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), background_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), layer_bounds);
}

TEST_F(PaintLayerClipperTest, LocalClipRectFixedUnderTransform) {
  SetBodyInnerHTML(R"HTML(
    <div id='transformed'
        style='will-change: transform; width: 100px; height: 100px;
        overflow: hidden'>
      <div id='fixed'
          style='position: fixed; width: 100px; height: 100px;
          top: -50px'>
       </div>
    </div>
  )HTML");

  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  PaintLayer* transformed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("transformed"))
          ->Layer();
  PaintLayer* fixed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("fixed"))->Layer();

  EXPECT_EQ(LayoutRect(0, 0, 100, 100),
            transformed->Clipper(PaintLayer::kUseGeometryMapper)
                .LocalClipRect(*transformed));
  EXPECT_EQ(LayoutRect(0, 50, 100, 100),
            fixed->Clipper(PaintLayer::kUseGeometryMapper)
                .LocalClipRect(*transformed));
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursive) {
  // SPv2 will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 5px; height: 5px; background: blue; overflow: hidden;
      position: relative;
    }
    </style>
    <div id='parent'>
      <div id='child'>
        <div id='grandchild'></div>
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();

  EXPECT_TRUE(parent->GetClipRectsCache());
  EXPECT_TRUE(child->GetClipRectsCache());

  parent->Clipper(PaintLayer::kUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  EXPECT_FALSE(parent->GetClipRectsCache());
  EXPECT_FALSE(child->GetClipRectsCache());
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursiveChild) {
  // SPv2 will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 5px; height: 5px; background: blue;
      position: relative;
    }
    </style>
    <div id='parent'>
      <div id='child'>
        <div id='grandchild'></div>
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();

  EXPECT_TRUE(parent->GetClipRectsCache());
  EXPECT_TRUE(child->GetClipRectsCache());

  child->Clipper(PaintLayer::kUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  EXPECT_TRUE(parent->GetClipRectsCache());
  EXPECT_FALSE(child->GetClipRectsCache());
}

TEST_F(PaintLayerClipperTest, CSSClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 400px; height: 400px; position: absolute;
        clip: rect(0, 50px, 100px, 0);
      }
    </style>
    <div id='target'></div>
  )HTML");

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(target, kUncachedClipRects);
  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  LayoutRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);

  EXPECT_EQ(LayoutRect(0, 0, 50, 100), background_rect.Rect());
  EXPECT_EQ(LayoutRect(0, 0, 50, 100), foreground_rect.Rect());
}

TEST_F(PaintLayerClipperTest, Filter) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0 }
      #target {
        filter: drop-shadow(0 3px 4px #333); overflow: hidden;
        width: 100px; height: 200px; border: 40px solid blue; margin: 50px;
      }
    </style>
    <div id='target'></div>
  )HTML");

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();

  // First test clip rects in the target layer itself.
  ClipRectsContext context(target, kUncachedClipRects);
  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  LayoutRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);

  // The background rect is used to clip stacking context (layer) output.
  // In this case, nothing is above us, thus the infinite rect. However we do
  // clip to the layer's after-filter visual rect as an optimization.
  EXPECT_EQ(LayoutRect(-12, -9, 204, 304), background_rect.Rect());
  // The foreground rect is used to clip the normal flow contents of the
  // stacking context (layer) thus including the overflow clip.
  EXPECT_EQ(LayoutRect(40, 40, 100, 200), foreground_rect.Rect());

  // Test without GeometryMapper.
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper(PaintLayer::kDoNotUseGeometryMapper)
      .CalculateRects(context, nullptr, infinite_rect, layer_bounds,
                      background_rect, foreground_rect);
  // The non-GeometryMapper path applies the immediate filter effect in
  // background rect.
  EXPECT_EQ(LayoutRect(-12, -9, 204, 304), background_rect.Rect());
  EXPECT_EQ(LayoutRect(40, 40, 100, 200), foreground_rect.Rect());

  // Test mapping to the root layer.
  ClipRectsContext root_context(GetLayoutView().Layer(), kUncachedClipRects);
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(root_context, &target->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);
  // This includes the filter effect because it's applied before mapping the
  // background rect to the root layer.
  EXPECT_EQ(LayoutRect(38, 41, 204, 304), background_rect.Rect());
  EXPECT_EQ(LayoutRect(90, 90, 100, 200), foreground_rect.Rect());

  // Test mapping to the root layer without GeometryMapper.
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper(PaintLayer::kDoNotUseGeometryMapper)
      .CalculateRects(root_context, nullptr, infinite_rect, layer_bounds,
                      background_rect, foreground_rect);
  EXPECT_EQ(LayoutRect(38, 41, 204, 304), background_rect.Rect());
  EXPECT_EQ(LayoutRect(90, 90, 100, 200), foreground_rect.Rect());
}

// Computed infinite clip rects may not match LayoutRect::InfiniteIntRect()
// due to floating point errors.
static bool IsInfinite(const LayoutRect& rect) {
  return rect.X().Round() < -10000000 && rect.MaxX().Round() > 10000000
      && rect.Y().Round() < -10000000 && rect.MaxY().Round() > 10000000;
}

TEST_F(PaintLayerClipperTest, IgnoreRootLayerClipWithCSSClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #root {
        width: 400px; height: 400px;
        position: absolute; clip: rect(0, 50px, 100px, 0);
      }
      #target {
        position: relative;
      }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* root =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("root"))->Layer();
  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(root, kPaintingClipRectsIgnoringOverflowClip);
  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  LayoutRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);

  EXPECT_TRUE(IsInfinite(background_rect.Rect()));
  EXPECT_TRUE(IsInfinite(foreground_rect.Rect()));
}

TEST_F(PaintLayerClipperTest, IgnoreRootLayerClipWithOverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #root {
        width: 400px; height: 400px;
        overflow: hidden;
      }
      #target {
        position: relative;
      }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* root =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("root"))->Layer();
  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(root, kPaintingClipRectsIgnoringOverflowClip);
  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  LayoutRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);

  EXPECT_TRUE(IsInfinite(background_rect.Rect()));
  EXPECT_TRUE(IsInfinite(foreground_rect.Rect()));
}

TEST_F(PaintLayerClipperTest, IgnoreRootLayerClipWithBothClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #root {
        width: 400px; height: 400px;
        position: absolute; clip: rect(0, 50px, 100px, 0);
        overflow: hidden;
      }
      #target {
        position: relative;
      }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* root =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("root"))->Layer();
  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(root, kPaintingClipRectsIgnoringOverflowClip);
  LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  LayoutRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      infinite_rect, layer_bounds, background_rect,
                      foreground_rect);

  EXPECT_TRUE(IsInfinite(background_rect.Rect()));
  EXPECT_TRUE(IsInfinite(foreground_rect.Rect()));
}

TEST_F(PaintLayerClipperTest, Fragmentation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=root style='width: 200px; height: 100px; columns: 2;
    column-gap: 0'>
      <div id=target style='width: 100px; height: 200px;
          background: lightblue; position: relative'>
      </div
    </div>
  )HTML");

  Element* root = GetDocument().getElementById("root");
  PaintLayer* root_paint_layer =
      ToLayoutBoxModelObject(root->GetLayoutObject())->Layer();
  ClipRectsContext context(root_paint_layer, kUncachedClipRects,
                           kIgnorePlatformOverlayScrollbarSize);
  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  EXPECT_TRUE(
      target_paint_layer->GetLayoutObject().FirstFragment().NextFragment());
  EXPECT_FALSE(target_paint_layer->GetLayoutObject()
                   .FirstFragment()
                   .NextFragment()
                   ->NextFragment());

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
                      background_rect, foreground_rect);

  EXPECT_EQ(LayoutRect(FloatRect(-1.0e6, -1.0e6, 1.0001e6, 1.0001e6)),
            background_rect.Rect());
  EXPECT_EQ(LayoutRect(FloatRect(-1.0e6, -1.0e6, 1.0001e6, 1.0001e6)),
            foreground_rect.Rect());
  EXPECT_EQ(LayoutRect(FloatRect(0, 0, 100, 200)), layer_bounds);

  target_paint_layer->Clipper(PaintLayer::kUseGeometryMapper)
      .CalculateRects(
          context,
          target_paint_layer->GetLayoutObject().FirstFragment().NextFragment(),
          LayoutRect(LayoutRect::InfiniteIntRect()), layer_bounds,
          background_rect, foreground_rect);

  EXPECT_EQ(LayoutRect(FloatRect(100, 0, 1000000, 999900)),
            background_rect.Rect());
  EXPECT_EQ(LayoutRect(FloatRect(100, 0, 1000000, 999900)),
            foreground_rect.Rect());
  // Layer bounds adjusted for pagination offset of second fragment.
  EXPECT_EQ(LayoutRect(FloatRect(100, -100, 100, 200)), layer_bounds);
}

}  // namespace blink
