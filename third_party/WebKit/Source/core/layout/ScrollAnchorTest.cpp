// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/layout/LayoutTestHelper.h"

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

}
