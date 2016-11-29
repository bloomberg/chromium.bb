// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBox.h"

#include "core/html/HTMLElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutBoxTest : public RenderingTest {};

TEST_F(LayoutBoxTest, BackgroundObscuredInRect) {
  setBodyInnerHTML(
      "<style>.column { width: 295.4px; padding-left: 10.4px; } "
      ".white-background { background: red; position: relative; overflow: "
      "hidden; border-radius: 1px; }"
      ".black-background { height: 100px; background: black; color: white; } "
      "</style>"
      "<div class='column'> <div> <div id='target' class='white-background'> "
      "<div class='black-background'></div> </div> </div> </div>");
  LayoutObject* layoutObject = getLayoutObjectByElementId("target");
  ASSERT_TRUE(layoutObject);
  ASSERT_TRUE(layoutObject->backgroundIsKnownToBeObscured());
}

TEST_F(LayoutBoxTest, BackgroundRect) {
  setBodyInnerHTML(
      "<style>div { position: absolute; width: 100px; height: 100px; padding: "
      "10px; border: 10px solid black; overflow: scroll; }"
      "#target1 { background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) border-box, green "
      "content-box;}"
      "#target2 { background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) content-box, green "
      "local border-box;}"
      "#target3 { background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) content-box, rgba(0, "
      "255, 0, 0.5) border-box;}"
      "#target4 { background-image: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg), none;"
      "           background-clip: content-box, border-box;"
      "           background-blend-mode: normal, multiply;"
      "           background-color: green; }"
      "#target5 { background: none border-box, green content-box;}"
      "#target6 { background: green content-box local; }"
      "</style>"
      "<div id='target1'></div>"
      "<div id='target2'></div>"
      "<div id='target3'></div>"
      "<div id='target4'></div>"
      "<div id='target5'></div>"
      "<div id='target6'></div>");

  // #target1's opaque background color only fills the content box but its
  // translucent image extends to the borders.
  LayoutBox* layoutBox = toLayoutBox(getLayoutObjectByElementId("target1"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layoutBox->backgroundRect(BackgroundClipRect));

  // #target2's background color is opaque but only fills the padding-box
  // because it has local attachment. This eclipses the content-box image.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target2"));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layoutBox->backgroundRect(BackgroundClipRect));

  // #target3's background color is not opaque so we only have a clip rect.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target3"));
  EXPECT_TRUE(layoutBox->backgroundRect(BackgroundKnownOpaqueRect).isEmpty());
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layoutBox->backgroundRect(BackgroundClipRect));

  // #target4's background color has a blend mode so it isn't opaque.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target4"));
  EXPECT_TRUE(layoutBox->backgroundRect(BackgroundKnownOpaqueRect).isEmpty());
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layoutBox->backgroundRect(BackgroundClipRect));

  // #target5's solid background only covers the content-box but it has a "none"
  // background covering the border box.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target5"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layoutBox->backgroundRect(BackgroundClipRect));

  // Because it can scroll due to local attachment, the opaque local background
  // in #target6 is treated as padding box for the clip rect, but remains the
  // content box for the known opaque rect.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target6"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layoutBox->backgroundRect(BackgroundClipRect));
}

TEST_F(LayoutBoxTest, LocationContainer) {
  setBodyInnerHTML(
      "<div id='div'>"
      "  <b>Inline content<img id='img'></b>"
      "</div>"
      "<table id='table'>"
      "  <tbody id='tbody'>"
      "    <tr id='row'>"
      "      <td id='cell' style='width: 100px; height: 80px'></td>"
      "    </tr>"
      "  </tbody>"
      "</table>");

  const LayoutBox* body = document().body()->layoutBox();
  const LayoutBox* div = toLayoutBox(getLayoutObjectByElementId("div"));
  const LayoutBox* img = toLayoutBox(getLayoutObjectByElementId("img"));
  const LayoutBox* table = toLayoutBox(getLayoutObjectByElementId("table"));
  const LayoutBox* tbody = toLayoutBox(getLayoutObjectByElementId("tbody"));
  const LayoutBox* row = toLayoutBox(getLayoutObjectByElementId("row"));
  const LayoutBox* cell = toLayoutBox(getLayoutObjectByElementId("cell"));

  EXPECT_EQ(body, div->locationContainer());
  EXPECT_EQ(div, img->locationContainer());
  EXPECT_EQ(body, table->locationContainer());
  EXPECT_EQ(table, tbody->locationContainer());
  EXPECT_EQ(tbody, row->locationContainer());
  EXPECT_EQ(tbody, cell->locationContainer());
}

TEST_F(LayoutBoxTest, TopLeftLocationFlipped) {
  setBodyInnerHTML(
      "<div style='width: 600px; height: 200px; writing-mode: vertical-rl'>"
      "  <div id='box1' style='width: 100px'></div>"
      "  <div id='box2' style='width: 200px'></div>"
      "</div>");

  const LayoutBox* box1 = toLayoutBox(getLayoutObjectByElementId("box1"));
  EXPECT_EQ(LayoutPoint(0, 0), box1->location());
  EXPECT_EQ(LayoutPoint(500, 0), box1->physicalLocation());

  const LayoutBox* box2 = toLayoutBox(getLayoutObjectByElementId("box2"));
  EXPECT_EQ(LayoutPoint(100, 0), box2->location());
  EXPECT_EQ(LayoutPoint(300, 0), box2->physicalLocation());
}

TEST_F(LayoutBoxTest, TableRowCellTopLeftLocationFlipped) {
  setBodyInnerHTML(
      "<div style='writing-mode: vertical-rl'>"
      "  <table style='border-spacing: 0'>"
      "    <thead><tr><td style='width: 50px'></td></tr></thead>"
      "    <tbody>"
      "      <tr id='row1'>"
      "        <td id='cell1' style='width: 100px; height: 80px'></td>"
      "      </tr>"
      "      <tr id='row2'>"
      "        <td id='cell2' style='width: 300px; height: 80px'></td>"
      "      </tr>"
      "    </tbody>"
      "  </table>"
      "</div>");

  // location and physicalLocation of a table row or a table cell should be
  // relative to the containing section.

  const LayoutBox* row1 = toLayoutBox(getLayoutObjectByElementId("row1"));
  EXPECT_EQ(LayoutPoint(0, 0), row1->location());
  EXPECT_EQ(LayoutPoint(300, 0), row1->physicalLocation());

  const LayoutBox* cell1 = toLayoutBox(getLayoutObjectByElementId("cell1"));
  EXPECT_EQ(LayoutPoint(0, 0), cell1->location());
  EXPECT_EQ(LayoutPoint(300, 0), cell1->physicalLocation());

  const LayoutBox* row2 = toLayoutBox(getLayoutObjectByElementId("row2"));
  EXPECT_EQ(LayoutPoint(100, 0), row2->location());
  EXPECT_EQ(LayoutPoint(0, 0), row2->physicalLocation());

  const LayoutBox* cell2 = toLayoutBox(getLayoutObjectByElementId("cell2"));
  EXPECT_EQ(LayoutPoint(100, 0), cell2->location());
  EXPECT_EQ(LayoutPoint(0, 0), cell2->physicalLocation());
}

TEST_F(LayoutBoxTest, LocationContainerOfSVG) {
  setBodyInnerHTML(
      "<svg id='svg' style='writing-mode:vertical-rl' width='500' height='500'>"
      "  <foreignObject x='44' y='77' width='100' height='80' id='foreign'>"
      "    <div id='child' style='width: 33px; height: 55px'>"
      "    </div>"
      "  </foreignObject>"
      "</svg>");
  const LayoutBox* svgRoot = toLayoutBox(getLayoutObjectByElementId("svg"));
  const LayoutBox* foreign = toLayoutBox(getLayoutObjectByElementId("foreign"));
  const LayoutBox* child = toLayoutBox(getLayoutObjectByElementId("child"));

  EXPECT_EQ(document().body()->layoutObject(), svgRoot->locationContainer());

  // The foreign object's location is not affected by SVGRoot's writing-mode.
  EXPECT_FALSE(foreign->locationContainer());
  EXPECT_EQ(LayoutRect(44, 77, 100, 80), foreign->frameRect());
  EXPECT_EQ(LayoutPoint(44, 77), foreign->physicalLocation());
  // The writing mode style should be still be inherited.
  EXPECT_TRUE(foreign->hasFlippedBlocksWritingMode());

  // The child of the foreign object is affected by writing-mode.
  EXPECT_EQ(foreign, child->locationContainer());
  EXPECT_EQ(LayoutRect(0, 0, 33, 55), child->frameRect());
  EXPECT_EQ(LayoutPoint(67, 0), child->physicalLocation());
  EXPECT_TRUE(child->hasFlippedBlocksWritingMode());
}

}  // namespace blink
