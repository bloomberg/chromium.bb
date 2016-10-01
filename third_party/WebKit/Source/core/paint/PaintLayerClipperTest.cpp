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
  PaintLayerClipperTest()
      : RenderingTest(SingleChildFrameLoaderClient::create()) {}
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

}  // namespace blink
