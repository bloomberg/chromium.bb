// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBox.h"

#include "build/build_config.h"
#include "core/html/HTMLElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutBoxTest : public RenderingTest {};

TEST_F(LayoutBoxTest, BackgroundObscuredInRect) {
  SetBodyInnerHTML(
      "<style>.column { width: 295.4px; padding-left: 10.4px; } "
      ".white-background { background: red; position: relative; overflow: "
      "hidden; border-radius: 1px; }"
      ".black-background { height: 100px; background: black; color: white; } "
      "</style>"
      "<div class='column'> <div> <div id='target' class='white-background'> "
      "<div class='black-background'></div> </div> </div> </div>");
  LayoutObject* layout_object = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->BackgroundIsKnownToBeObscured());
}

TEST_F(LayoutBoxTest, BackgroundRect) {
  SetBodyInnerHTML(
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
  LayoutBox* layout_box = ToLayoutBox(GetLayoutObjectByElementId("target1"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layout_box->BackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layout_box->BackgroundRect(kBackgroundClipRect));

  // #target2's background color is opaque but only fills the padding-box
  // because it has local attachment. This eclipses the content-box image.
  layout_box = ToLayoutBox(GetLayoutObjectByElementId("target2"));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layout_box->BackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layout_box->BackgroundRect(kBackgroundClipRect));

  // #target3's background color is not opaque so we only have a clip rect.
  layout_box = ToLayoutBox(GetLayoutObjectByElementId("target3"));
  EXPECT_TRUE(layout_box->BackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layout_box->BackgroundRect(kBackgroundClipRect));

  // #target4's background color has a blend mode so it isn't opaque.
  layout_box = ToLayoutBox(GetLayoutObjectByElementId("target4"));
  EXPECT_TRUE(layout_box->BackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layout_box->BackgroundRect(kBackgroundClipRect));

  // #target5's solid background only covers the content-box but it has a "none"
  // background covering the border box.
  layout_box = ToLayoutBox(GetLayoutObjectByElementId("target5"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layout_box->BackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layout_box->BackgroundRect(kBackgroundClipRect));

  // Because it can scroll due to local attachment, the opaque local background
  // in #target6 is treated as padding box for the clip rect, but remains the
  // content box for the known opaque rect.
  layout_box = ToLayoutBox(GetLayoutObjectByElementId("target6"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layout_box->BackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layout_box->BackgroundRect(kBackgroundClipRect));
}

TEST_F(LayoutBoxTest, LocationContainer) {
  SetBodyInnerHTML(
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

  const LayoutBox* body = GetDocument().body()->GetLayoutBox();
  const LayoutBox* div = ToLayoutBox(GetLayoutObjectByElementId("div"));
  const LayoutBox* img = ToLayoutBox(GetLayoutObjectByElementId("img"));
  const LayoutBox* table = ToLayoutBox(GetLayoutObjectByElementId("table"));
  const LayoutBox* tbody = ToLayoutBox(GetLayoutObjectByElementId("tbody"));
  const LayoutBox* row = ToLayoutBox(GetLayoutObjectByElementId("row"));
  const LayoutBox* cell = ToLayoutBox(GetLayoutObjectByElementId("cell"));

  EXPECT_EQ(body, div->LocationContainer());
  EXPECT_EQ(div, img->LocationContainer());
  EXPECT_EQ(body, table->LocationContainer());
  EXPECT_EQ(table, tbody->LocationContainer());
  EXPECT_EQ(tbody, row->LocationContainer());
  EXPECT_EQ(tbody, cell->LocationContainer());
}

TEST_F(LayoutBoxTest, TopLeftLocationFlipped) {
  SetBodyInnerHTML(
      "<div style='width: 600px; height: 200px; writing-mode: vertical-rl'>"
      "  <div id='box1' style='width: 100px'></div>"
      "  <div id='box2' style='width: 200px'></div>"
      "</div>");

  const LayoutBox* box1 = ToLayoutBox(GetLayoutObjectByElementId("box1"));
  EXPECT_EQ(LayoutPoint(0, 0), box1->Location());
  EXPECT_EQ(LayoutPoint(500, 0), box1->PhysicalLocation());

  const LayoutBox* box2 = ToLayoutBox(GetLayoutObjectByElementId("box2"));
  EXPECT_EQ(LayoutPoint(100, 0), box2->Location());
  EXPECT_EQ(LayoutPoint(300, 0), box2->PhysicalLocation());
}

TEST_F(LayoutBoxTest, TableRowCellTopLeftLocationFlipped) {
  SetBodyInnerHTML(
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

  const LayoutBox* row1 = ToLayoutBox(GetLayoutObjectByElementId("row1"));
  EXPECT_EQ(LayoutPoint(0, 0), row1->Location());
  EXPECT_EQ(LayoutPoint(300, 0), row1->PhysicalLocation());

  const LayoutBox* cell1 = ToLayoutBox(GetLayoutObjectByElementId("cell1"));
  EXPECT_EQ(LayoutPoint(0, 0), cell1->Location());
  EXPECT_EQ(LayoutPoint(300, 0), cell1->PhysicalLocation());

  const LayoutBox* row2 = ToLayoutBox(GetLayoutObjectByElementId("row2"));
  EXPECT_EQ(LayoutPoint(100, 0), row2->Location());
  EXPECT_EQ(LayoutPoint(0, 0), row2->PhysicalLocation());

  const LayoutBox* cell2 = ToLayoutBox(GetLayoutObjectByElementId("cell2"));
  EXPECT_EQ(LayoutPoint(100, 0), cell2->Location());
  EXPECT_EQ(LayoutPoint(0, 0), cell2->PhysicalLocation());
}

TEST_F(LayoutBoxTest, LocationContainerOfSVG) {
  SetBodyInnerHTML(
      "<svg id='svg' style='writing-mode:vertical-rl' width='500' height='500'>"
      "  <foreignObject x='44' y='77' width='100' height='80' id='foreign'>"
      "    <div id='child' style='width: 33px; height: 55px'>"
      "    </div>"
      "  </foreignObject>"
      "</svg>");
  const LayoutBox* svg_root = ToLayoutBox(GetLayoutObjectByElementId("svg"));
  const LayoutBox* foreign = ToLayoutBox(GetLayoutObjectByElementId("foreign"));
  const LayoutBox* child = ToLayoutBox(GetLayoutObjectByElementId("child"));

  EXPECT_EQ(GetDocument().body()->GetLayoutObject(),
            svg_root->LocationContainer());

  // The foreign object's location is not affected by SVGRoot's writing-mode.
  EXPECT_FALSE(foreign->LocationContainer());
  EXPECT_EQ(LayoutRect(44, 77, 100, 80), foreign->FrameRect());
  EXPECT_EQ(LayoutPoint(44, 77), foreign->PhysicalLocation());
  // The writing mode style should be still be inherited.
  EXPECT_TRUE(foreign->HasFlippedBlocksWritingMode());

  // The child of the foreign object is affected by writing-mode.
  EXPECT_EQ(foreign, child->LocationContainer());
  EXPECT_EQ(LayoutRect(0, 0, 33, 55), child->FrameRect());
  EXPECT_EQ(LayoutPoint(67, 0), child->PhysicalLocation());
  EXPECT_TRUE(child->HasFlippedBlocksWritingMode());
}

TEST_F(LayoutBoxTest, ControlClip) {
  SetBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "  #target {"
      "    position: relative;"
      "    width: 100px; height: 50px;"
      "  }"
      "</style>"
      "<input id='target' type='button' value='some text'/>");
  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  EXPECT_TRUE(target->HasControlClip());
  EXPECT_TRUE(target->HasClipRelatedProperty());
  EXPECT_TRUE(target->ShouldClipOverflow());
#if defined(OS_MACOSX)
  EXPECT_EQ(LayoutRect(0, 0, 100, 18), target->ClippingRect());
#else
  EXPECT_EQ(LayoutRect(2, 2, 96, 46), target->ClippingRect());
#endif
}

TEST_F(LayoutBoxTest, LocalVisualRectWithMask) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(
      "<div id='target' style='-webkit-mask-image: url(#a);"
      "     width: 100px; height: 100px; background: blue'>"
      "  <div style='width: 300px; height: 10px; background: green'></div>"
      "</div>");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  EXPECT_TRUE(target->HasMask());
  EXPECT_EQ(LayoutRect(0, 0, 300, 100), target->LocalVisualRect());
}

TEST_F(LayoutBoxTest, LocalVisualRectWithMaskAndOverflowClip) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(
      "<div id='target' style='-webkit-mask-image: url(#a); overflow: hidden;"
      "     width: 100px; height: 100px; background: blue'>"
      "  <div style='width: 300px; height: 10px; background: green'></div>"
      "</div>");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  EXPECT_TRUE(target->HasMask());
  EXPECT_TRUE(target->HasOverflowClip());
  EXPECT_EQ(LayoutRect(0, 0, 100, 100), target->LocalVisualRect());
}

TEST_F(LayoutBoxTest, ContentsVisualOverflowPropagation) {
  SetBodyInnerHTML(
      "<style>"
      "  div { width: 100px; height: 100px }"
      "</style>"
      "<div id='a'>"
      "  <div style='height: 50px'></div>"
      "  <div id='b' style='writing-mode: vertical-rl; margin-left: 60px'>"
      "    <div style='width: 30px'></div>"
      "    <div id='c' style='margin-top: 40px'>"
      "      <div style='width: 10px'></div>"
      "      <div style='margin-top: 20px; margin-left: 10px'></div>"
      "    </div>"
      "    <div id='d' style='writing-mode: vertical-lr; margin-top: 40px'>"
      "      <div style='width: 10px'></div>"
      "      <div style='margin-top: 20px'></div>"
      "    </div>"
      "  </div>"
      "</div>");

  auto* c = ToLayoutBox(GetLayoutObjectByElementId("c"));
  EXPECT_EQ(LayoutRect(0, 0, 100, 100), c->SelfVisualOverflowRect());
  EXPECT_EQ(LayoutRect(10, 20, 100, 100), c->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(0, 0, 110, 120), c->VisualOverflowRect());
  // C and its parent b have the same blocks direction.
  EXPECT_EQ(LayoutRect(0, 0, 110, 120), c->VisualOverflowRectForPropagation());

  auto* d = ToLayoutBox(GetLayoutObjectByElementId("d"));
  EXPECT_EQ(LayoutRect(0, 0, 100, 100), d->SelfVisualOverflowRect());
  EXPECT_EQ(LayoutRect(10, 20, 100, 100), d->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(0, 0, 110, 120), d->VisualOverflowRect());
  // D and its parent b have different blocks direction.
  EXPECT_EQ(LayoutRect(-10, 0, 110, 120),
            d->VisualOverflowRectForPropagation());

  auto* b = ToLayoutBox(GetLayoutObjectByElementId("b"));
  EXPECT_EQ(LayoutRect(0, 0, 100, 100), b->SelfVisualOverflowRect());
  // Union of VisualOverflowRectForPropagations offset by locations of c and d.
  EXPECT_EQ(LayoutRect(30, 40, 200, 120), b->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(0, 0, 230, 160), b->VisualOverflowRect());
  // B and its parent A have different blocks direction.
  EXPECT_EQ(LayoutRect(-130, 0, 230, 160),
            b->VisualOverflowRectForPropagation());

  auto* a = ToLayoutBox(GetLayoutObjectByElementId("a"));
  EXPECT_EQ(LayoutRect(0, 0, 100, 100), a->SelfVisualOverflowRect());
  EXPECT_EQ(LayoutRect(-70, 50, 230, 160), a->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(-70, 0, 230, 210), a->VisualOverflowRect());
}

}  // namespace blink
