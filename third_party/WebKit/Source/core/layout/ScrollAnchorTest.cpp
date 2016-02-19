// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayerScrollableArea.h"

namespace blink {

class ScrollAnchorTest : public RenderingTest {
public:
    ScrollAnchorTest() { RuntimeEnabledFeatures::setScrollAnchoringEnabled(true); }
    ~ScrollAnchorTest() { RuntimeEnabledFeatures::setScrollAnchoringEnabled(false); }
};

TEST_F(ScrollAnchorTest, Basic)
{
    setBodyInnerHTML(
        "<style> body { height: 1000px } div { height: 100px } </style>"
        "<div id='block1'>abc</div>"
        "<div id='block2'>def</div>");

    ScrollableArea* viewport = document().view()->layoutViewportScrollableArea();
    viewport->scrollBy(DoubleSize(0, 150), UserScroll);
    document().getElementById("block1")->setAttribute(HTMLNames::styleAttr, "height: 200px");
    document().view()->updateAllLifecyclePhases();
    EXPECT_EQ(250, viewport->scrollPosition().y());
}

TEST_F(ScrollAnchorTest, AnchorWithLayerInScrollingDiv)
{
    setBodyInnerHTML(
        "<style>"
        "    #scroller { overflow: scroll; width: 500px; height: 400px; }"
        "    div { height: 100px }"
        "    #block2 { overflow: hidden }"
        "    #space { height: 1000px; }"
        "</style>"
        "<div id='scroller'><div id='space'>"
        "<div id='block1'>abc</div>"
        "<div id='block2'>def</div>"
        "</div></div>");

    PaintLayerScrollableArea* scroller = toLayoutBox(
        document().getElementById("scroller")->layoutObject())->scrollableArea();
    scroller->scrollBy(DoubleSize(0, 150), UserScroll);
    document().getElementById("block1")->setAttribute(HTMLNames::styleAttr, "height: 200px");

    // In this layout pass we will anchor to #block2 which has its own PaintLayer.
    document().view()->updateAllLifecyclePhases();
    EXPECT_EQ(250, scroller->scrollPosition().y());
    EXPECT_EQ(document().getElementById("block2")->layoutObject(),
        scroller->scrollAnchor().anchorObject());

    // Test that the anchor object can be destroyed without affecting the scroll position.
    document().getElementById("block2")->remove();
    document().view()->updateAllLifecyclePhases();
    EXPECT_EQ(250, scroller->scrollPosition().y());
}

}
