// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerClipper.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

class PaintLayerClipperTest : public RenderingTest {
 public:
  PaintLayerClipperTest() : RenderingTest(EmptyFrameLoaderClient::create()) {}
};

TEST_F(PaintLayerClipperTest, LayoutSVGRoot) {
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<svg id=target width=200 height=300 style='position: relative'>"
      "  <rect width=400 height=500 fill='blue'/>"
      "</svg>");

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
  targetPaintLayer->clipper().calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(LayoutRect::infiniteIntRect()), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(LayoutRect::infiniteIntRect()), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), layerBounds);
}

TEST_F(PaintLayerClipperTest, LayoutSVGRootChild) {
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
  targetPaintLayer->clipper().calculateRects(
      context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds,
      backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 200, 300), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(8, 8, 400, 0), layerBounds);
}

TEST_F(PaintLayerClipperTest, ContainPaintClip) {
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
  layer->clipper().calculateRects(context, infiniteRect, layerBounds,
                                  backgroundRect, foregroundRect);
  EXPECT_EQ(infiniteRect, backgroundRect.rect());
  EXPECT_EQ(infiniteRect, foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), layerBounds);

  ClipRectsContext contextClip(layer, PaintingClipRects);
  layer->clipper().calculateRects(contextClip, infiniteRect, layerBounds,
                                  backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), layerBounds);
}

TEST_F(PaintLayerClipperTest, NestedContainPaintClip) {
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
  layer->clipper().calculateRects(context, infiniteRect, layerBounds,
                                  backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), layerBounds);

  ClipRectsContext contextClip(layer->parent(), PaintingClipRects);
  layer->clipper().calculateRects(contextClip, infiniteRect, layerBounds,
                                  backgroundRect, foregroundRect);
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), backgroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 200), foregroundRect.rect());
  EXPECT_EQ(LayoutRect(0, 0, 200, 400), layerBounds);
}

TEST_F(PaintLayerClipperTest, LocalClipRectFixedUnderTransform) {
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

  EXPECT_EQ(LayoutRect(0, 0, 100, 100),
            transformed->clipper().localClipRect(transformed));
  EXPECT_EQ(LayoutRect(0, 50, 100, 100),
            fixed->clipper().localClipRect(transformed));
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursive) {
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

  parent->clipper().clearClipRectsIncludingDescendants();

  EXPECT_FALSE(parent->clipRectsCache());
  EXPECT_FALSE(child->clipRectsCache());
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursiveChild) {
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

  child->clipper().clearClipRectsIncludingDescendants();

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_FALSE(child->clipRectsCache());
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursiveOneType) {
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

  parent->clipper().clearClipRectsIncludingDescendants(AbsoluteClipRects);

  EXPECT_TRUE(parent->clipRectsCache());
  EXPECT_TRUE(child->clipRectsCache());
  EXPECT_FALSE(parent->clipRectsCache()->get(AbsoluteClipRects).root);
  EXPECT_FALSE(parent->clipRectsCache()->get(AbsoluteClipRects).root);
}

}  // namespace blink
