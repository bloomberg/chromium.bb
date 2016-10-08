// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayer.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"

namespace blink {

using PaintLayerTest = RenderingTest;

TEST_F(PaintLayerTest, PaintingExtentReflection) {
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

TEST_F(PaintLayerTest, PaintingExtentReflectionWithTransform) {
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

}  // namespace blink
