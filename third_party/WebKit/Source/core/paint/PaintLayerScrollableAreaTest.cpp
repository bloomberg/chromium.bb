// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerScrollableAreaTest.h"

namespace blink {

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersPromoted)
{
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

    setBodyInnerHTML(
        "<style>"
        "#scroller { overflow: scroll; height: 300px; width: 300px; background-color: rgb(0,128,0); }"
        "#scrolled { height: 1000px; width: 250px; }"
        "</style>"
        "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
    document().view()->updateAllLifecyclePhases();

    ASSERT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
    Element* scroller = document().getElementById("scroller");
    PaintLayer* paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    ASSERT_TRUE(paintLayer->needsCompositedScrolling());
}

TEST_F(PaintLayerScrollableAreaTest, TransparentLayersNotPromoted)
{
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

    setBodyInnerHTML(
        "<style>"
        "#scroller { overflow: scroll; height: 300px; width: 300px; background-color: rgba(0,128,0,0.5); }"
        "#scrolled { height: 1000px; width: 250px; }"
        "</style>"
        "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
    document().view()->updateAllLifecyclePhases();

    ASSERT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
    Element* scroller = document().getElementById("scroller");
    PaintLayer* paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    ASSERT_TRUE(!paintLayer->needsCompositedScrolling());
}

}
