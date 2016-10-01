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

  // #target1's opaque background color only fills the content box but its translucent image extends to the borders.
  LayoutBox* layoutBox = toLayoutBox(getLayoutObjectByElementId("target1"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layoutBox->backgroundRect(BackgroundClipRect));

  // #target2's background color is opaque but only fills the padding-box because it has local attachment.
  // This eclipses the content-box image.
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

  // #target5's solid background only covers the content-box but it has a "none" background
  // covering the border box.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target5"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(0, 0, 140, 140),
            layoutBox->backgroundRect(BackgroundClipRect));

  // Because it can scroll due to local attachment, the opaque local background in #target6
  // is treated as padding box for the clip rect, but remains the content box for the known
  // opaque rect.
  layoutBox = toLayoutBox(getLayoutObjectByElementId("target6"));
  EXPECT_EQ(LayoutRect(20, 20, 100, 100),
            layoutBox->backgroundRect(BackgroundKnownOpaqueRect));
  EXPECT_EQ(LayoutRect(10, 10, 120, 120),
            layoutBox->backgroundRect(BackgroundClipRect));
}

}  // namespace blink
