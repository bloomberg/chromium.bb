// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerClipper.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/LayoutTestSupport.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"

namespace blink {

class PaintLayerClipperTest : public ::testing::WithParamInterface<bool>,
                              private ScopedSlimmingPaintV2ForTest,
                              public RenderingTest {
 public:
  PaintLayerClipperTest()
      : ScopedSlimmingPaintV2ForTest(GetParam()),
        RenderingTest(EmptyLocalFrameClient::create()) {}

  void SetUp() override {
    LayoutTestSupport::setMockThemeEnabledForTest(true);
    RenderingTest::SetUp();
  }

  void TearDown() override {
    LayoutTestSupport::setMockThemeEnabledForTest(false);
    RenderingTest::TearDown();
  }

  bool geometryMapperCacheEmpty(const PaintLayerClipper& clipper) {
    return clipper.m_geometryMapper->m_transformCache.isEmpty() &&
           clipper.m_geometryMapper->m_clipCache.isEmpty();
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        PaintLayerClipperTest,
                        ::testing::ValuesIn(std::vector<bool>{false, true}));

TEST_P(PaintLayerClipperTest, LayoutSVGRoot) {
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<svg id=target width=200 height=300 style='position: relative'>"
      "  <rect width=400 height=500 fill='blue'/>"
      "</svg>");

  Element* target = document().getElementById("target");
  PaintLayer* targetPaintLayer =
      toLayoutBoxModelObject(target->layoutObject())->layer();
  ClipRectsContext context(document().layoutView()->layer(), UncachedClipRects,
                           IgnoreOverlayScrollbarSize,
                           LayoutSize(FloatSize(0.25, 0.35)));
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    context.setIgnoreOverflowClip();
  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  targetPaintLayer->clipper(option).calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(FloatRect(8.25, 8.35, 200, 300)), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(FloatRect(8.25, 8.35, 200, 300)), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), layerBounds);
}

TEST_P(PaintLayerClipperTest, ControlClip) {
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<input id=target style='position:absolute; width: 200px; height: 300px'"
      "    type=button>");
  Element* target = document().getElementById("target");
  PaintLayer* targetPaintLayer =
      toLayoutBoxModelObject(target->layoutObject())->layer();
  ClipRectsContext context(document().layoutView()->layer(), UncachedClipRects);
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    context.setIgnoreOverflowClip();
  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  targetPaintLayer->clipper(option).calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);
#if OS(MACOSX)
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(LayoutRect(3, 4, 210, 28), backgroundRect.rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(LayoutRect(8, 8, 200, 18), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 18), layerBounds);
#else
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), backgroundRect.rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(LayoutRect(10, 10, 196, 296), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), layerBounds);
#endif
}

TEST_P(PaintLayerClipperTest, RoundedClip) {
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div id='target' style='position:absolute; width: 200px; height: 300px;"
      "    overflow: hidden; border-radius: 1px'>"
      "</div>");

  Element* target = document().getElementById("target");
  PaintLayer* targetPaintLayer =
      toLayoutBoxModelObject(target->layoutObject())->layer();
  ClipRectsContext context(document().layoutView()->layer(), UncachedClipRects);
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    context.setIgnoreOverflowClip();

  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  targetPaintLayer->clipper(option).calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);

  // Only the foreground rect gets hasRadius set for overflow clipping
  // of descendants.
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), backgroundRect.rect());
  EXPECT_FALSE(backgroundRect.hasRadius());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), foregroundRect.rect());
  EXPECT_TRUE(foregroundRect.hasRadius());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), layerBounds);
}

TEST_P(PaintLayerClipperTest, RoundedClipNested) {
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div id='parent' style='position:absolute; width: 200px; height: 300px;"
      "    overflow: hidden; border-radius: 1px'>"
      "  <div id='child' style='position: relative; width: 500px; "
      "       height: 500px'>"
      "  </div>"
      "</div>");

  Element* parent = document().getElementById("parent");
  PaintLayer* parentPaintLayer =
      toLayoutBoxModelObject(parent->layoutObject())->layer();

  Element* child = document().getElementById("child");
  PaintLayer* childPaintLayer =
      toLayoutBoxModelObject(child->layoutObject())->layer();

  ClipRectsContext context(parentPaintLayer, UncachedClipRects);

  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  childPaintLayer->clipper(option).calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);

  EXPECT_EQ(LayoutRect(0, 0, 200, 300), backgroundRect.rect());
  EXPECT_TRUE(backgroundRect.hasRadius());
  EXPECT_EQ(LayoutRect(0, 0, 200, 300), foregroundRect.rect());
  EXPECT_TRUE(foregroundRect.hasRadius());
  EXPECT_EQ(LayoutRect(0, 0, 500, 500), layerBounds);
}

TEST_P(PaintLayerClipperTest, ControlClipSelect) {
  setBodyInnerHTML(
      "<select id='target' style='position: relative; width: 100px; "
      "    background: none; border: none; padding: 0px 15px 0px 5px;'>"
      "  <option>"
      "    Test long texttttttttttttttttttttttttttttttt"
      "  </option>"
      "</select>");
  Element* target = document().getElementById("target");
  PaintLayer* targetPaintLayer =
      toLayoutBoxModelObject(target->layoutObject())->layer();
  ClipRectsContext context(document().layoutView()->layer(), UncachedClipRects);
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    context.setIgnoreOverflowClip();
  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  targetPaintLayer->clipper(option).calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);
// The control clip for a select excludes the area for the down arrow.
#if OS(MACOSX)
  EXPECT_EQ(LayoutRect(16, 9, 79, 13), foregroundRect.rect());
#elif OS(WIN)
  EXPECT_EQ(LayoutRect(17, 9, 60, 16), foregroundRect.rect());
#else
  EXPECT_EQ(LayoutRect(17, 9, 60, 15), foregroundRect.rect());
#endif
}

TEST_P(PaintLayerClipperTest, LayoutSVGRootChild) {
  setBodyInnerHTML(
      "<svg width=200 height=300 style='position: relative'>"
      "  <foreignObject width=400 height=500>"
      "    <div id=target xmlns='http://www.w3.org/1999/xhtml' "
      "style='position: relative'></div>"
      "  </foreignObject>"
      "</svg>");

  Element* target = document().getElementById("target");
  PaintLayer* targetPaintLayer =
      toLayoutBoxModelObject(target->layoutObject())->layer();
  ClipRectsContext context(document().layoutView()->layer(), UncachedClipRects);
  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  targetPaintLayer->clipper(option).calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 400, 0), layerBounds);
}

TEST_P(PaintLayerClipperTest, ContainPaintClip) {
  setBodyInnerHTML(
      "<div id='target'"
      "    style='contain: paint; width: 200px; height: 200px; overflow: auto'>"
      "  <div style='height: 400px'></div>"
      "</div>");

  LayoutRect infiniteRect(LayoutRect::infiniteIntRect());
  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  ClipRectsContext context(layer, PaintingClipRectsIgnoringOverflowClip);
  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  layer->clipper(option).calculateRects(context, infiniteRect, layerBounds,
                                        backgroundRect, foregroundRect);
  EXPECT_GE(backgroundRect.rect().size().width().toInt(), 33554422);
  EXPECT_GE(backgroundRect.rect().size().height().toInt(), 33554422);
  EXPECT_EQ(backgroundRect.rect(), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), layerBounds);

  ClipRectsContext contextClip(layer, PaintingClipRects);

  layer->clipper(option).calculateRects(contextClip, infiniteRect, layerBounds,
                                        backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), layerBounds);
}

TEST_P(PaintLayerClipperTest, NestedContainPaintClip) {
  setBodyInnerHTML(
      "<div style='contain: paint; width: 200px; height: 200px; overflow: "
      "auto'>"
      "  <div id='target' style='contain: paint; height: 400px'>"
      "  </div>"
      "</div>");

  LayoutRect infiniteRect(LayoutRect::infiniteIntRect());
  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  ClipRectsContext context(layer->parent(),
                           PaintingClipRectsIgnoringOverflowClip);
  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  layer->clipper(option).calculateRects(context, infiniteRect, layerBounds,
                                        backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), layerBounds);

  ClipRectsContext contextClip(layer->parent(), PaintingClipRects);

  layer->clipper(option).calculateRects(contextClip, infiniteRect, layerBounds,
                                        backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), layerBounds);
}

TEST_P(PaintLayerClipperTest, LocalClipRectFixedUnderTransform) {
  setBodyInnerHTML(
      "<div id='transformed'"
      "    style='will-change: transform; width: 100px; height: 100px;"
      "    overflow: hidden'>"
      "  <div id='fixed' "
      "      style='position: fixed; width: 100px; height: 100px;"
      "      top: -50px'>"
      "   </div>"
      "</div>");

  LayoutRect infiniteRect(LayoutRect::infiniteIntRect());
  PaintLayer* transformed =
      toLayoutBoxModelObject(getLayoutObjectByElementId("transformed"))
          ->layer();
  PaintLayer* fixed =
      toLayoutBoxModelObject(getLayoutObjectByElementId("fixed"))->layer();

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  EXPECT_EQ(LayoutRect(0, 0, 100, 100),
            transformed->clipper(option).localClipRect(*transformed));
  EXPECT_EQ(LayoutRect(0, 50, 100, 100),
            fixed->clipper(option).localClipRect(*transformed));
}

TEST_P(PaintLayerClipperTest, ClearClipRectsRecursive) {
  // SPv2 will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      "<style>"
      "div { "
      "  width: 5px; height: 5px; background: blue; overflow: hidden;"
      "  position: relative;"
      "}"
      "</style>"
      "<div id='parent'>"
      "  <div id='child'>"
      "    <div id='grandchild'></div>"
      "  </div>"
      "</div>");

  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_TRUE(child->clipRectsCache());

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  parent->clipper(option).clearClipRectsIncludingDescendants();

  EXPECT_FALSE(parent->clipRectsCache());
  EXPECT_FALSE(child->clipRectsCache());
}

TEST_P(PaintLayerClipperTest, ClearClipRectsRecursiveChild) {
  // SPv2 will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      "<style>"
      "div { "
      "  width: 5px; height: 5px; background: blue;"
      "  position: relative;"
      "}"
      "</style>"
      "<div id='parent'>"
      "  <div id='child'>"
      "    <div id='grandchild'></div>"
      "  </div>"
      "</div>");

  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_TRUE(child->clipRectsCache());

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  child->clipper(option).clearClipRectsIncludingDescendants();

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_FALSE(child->clipRectsCache());
}

TEST_P(PaintLayerClipperTest, ClearClipRectsRecursiveOneType) {
  // SPv2 will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    return;

  setBodyInnerHTML(
      "<style>"
      "div { "
      "  width: 5px; height: 5px; background: blue;"
      "  position: relative;"
      "}"
      "</style>"
      "<div id='parent'>"
      "  <div id='child'>"
      "    <div id='grandchild'></div>"
      "  </div>"
      "</div>");

  PaintLayer* parent =
      toLayoutBoxModelObject(getLayoutObjectByElementId("parent"))->layer();
  PaintLayer* child =
      toLayoutBoxModelObject(getLayoutObjectByElementId("child"))->layer();

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_TRUE(child->clipRectsCache());
  EXPECT_TRUE(parent->clipRectsCache()->get(AbsoluteClipRects).root);
  EXPECT_TRUE(child->clipRectsCache()->get(AbsoluteClipRects).root);

  PaintLayer::GeometryMapperOption option = PaintLayer::DoNotUseGeometryMapper;
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    option = PaintLayer::UseGeometryMapper;
  parent->clipper(option).clearClipRectsIncludingDescendants(AbsoluteClipRects);

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_TRUE(child->clipRectsCache());
  EXPECT_FALSE(parent->clipRectsCache()->get(AbsoluteClipRects).root);
  EXPECT_FALSE(parent->clipRectsCache()->get(AbsoluteClipRects).root);
}

}  // namespace blink
