// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerScrollableAreaTest.h"

#include "platform/graphics/GraphicsLayer.h"

namespace blink {

TEST_F(PaintLayerScrollableAreaTest, ShouldPaintBackgroundOntoScrollingContentsLayer)
{
    document().frame()->settings()->setPreferCompositingToLCDTextEnabled(true);
    setBodyInnerHTML(
        "<style>.scroller { overflow: scroll; will-change: transform; width: 300px; height: 300px;} .spacer { height: 1000px; }</style>"
        "<div id='scroller1' class='scroller' style='background: white local;'>"
        "    <div id='negative-composited-child' style='background-color: red; width: 1px; height: 1px; position: absolute; backface-visibility: hidden; z-index: -1'></div>"
        "    <div class='spacer'></div>"
        "</div>"
        "<div id='scroller2' class='scroller' style='background: white content-box; padding: 10px;'><div class='spacer'></div></div>"
        "<div id='scroller3' class='scroller' style='background: white local content-box; padding: 10px;'><div class='spacer'></div></div>"
        "<div id='scroller4' class='scroller' style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg), white local;'><div class='spacer'></div></div>"
        "<div id='scroller5' class='scroller' style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white local;'><div class='spacer'></div></div>"
        "<div id='scroller6' class='scroller' style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white padding-box; padding: 10px;'><div class='spacer'></div></div>"
        "<div id='scroller7' class='scroller' style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white content-box; padding: 10px;'><div class='spacer'></div></div>"
        );

    // First scroller cannot paint background into scrolling contents layer because it has a negative z-index child.
    EXPECT_FALSE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller1"));

    // Second scroller cannot paint background into scrolling contents layer because it has a content-box clip without local attachment.
    EXPECT_FALSE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller2"));

    // Third scroller can paint background into scrolling contents layer.
    EXPECT_TRUE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller3"));

    // Fourth scroller cannot paint background into scrolling contents layer because the background image is not locally attached.
    EXPECT_FALSE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller4"));

    // Fifth scroller can paint background into scrolling contents layer because both the image and color are locally attached.
    EXPECT_TRUE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller5"));

    // Sixth scroller can paint background into scrolling contents layer because the image is locally attached and even though
    // the color is not, it is filled to the padding box so it will be drawn the same as a locally attached background.
    EXPECT_TRUE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller6"));

    // Seventh scroller cannot paint background into scrolling contents layer because the color is filled to the content
    // box and we have padding so it is not equivalent to a locally attached background.
    EXPECT_FALSE(shouldPaintBackgroundOntoScrollingContentsLayer("scroller7"));
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersPromoted)
{
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

    setBodyInnerHTML(
        "<style>"
        "#scroller { overflow: scroll; height: 200px; width: 200px; background: white local content-box; border: 10px solid rgba(0, 255, 0, 0.5); }"
        "#scrolled { height: 300px; }"
        "</style>"
        "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
    Element* scroller = document().getElementById("scroller");
    PaintLayer* paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    EXPECT_TRUE(paintLayer->needsCompositedScrolling());
    EXPECT_TRUE(paintLayer->graphicsLayerBacking());
    ASSERT_TRUE(paintLayer->graphicsLayerBackingForScrolling());
    EXPECT_TRUE(paintLayer->graphicsLayerBackingForScrolling()->contentsOpaque());
}

TEST_F(PaintLayerScrollableAreaTest, TransparentLayersNotPromoted)
{
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

    setBodyInnerHTML(
        "<style>"
        "#scroller { overflow: scroll; height: 200px; width: 200px; background: rgba(0, 255, 0, 0.5) local content-box; border: 10px solid rgba(0, 255, 0, 0.5); }"
        "#scrolled { height: 300px; }"
        "</style>"
        "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
    Element* scroller = document().getElementById("scroller");
    PaintLayer* paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    EXPECT_FALSE(paintLayer->needsCompositedScrolling());
    EXPECT_FALSE(paintLayer->graphicsLayerBacking());
    EXPECT_FALSE(paintLayer->graphicsLayerBackingForScrolling());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersDepromotedOnStyleChange)
{
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

    setBodyInnerHTML(
        "<style>"
        "#scroller { overflow: scroll; height: 200px; width: 200px; background: white local content-box; }"
        "#scrolled { height: 300px; }"
        "</style>"
        "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
    Element* scroller = document().getElementById("scroller");
    PaintLayer* paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    EXPECT_TRUE(paintLayer->needsCompositedScrolling());

    // Change the background to transparent
    scroller->setAttribute(HTMLNames::styleAttr, "background: rgba(255,255,255,0.5) local content-box;");
    document().view()->updateAllLifecyclePhases();
    paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    EXPECT_FALSE(paintLayer->needsCompositedScrolling());
    EXPECT_FALSE(paintLayer->graphicsLayerBacking());
    EXPECT_FALSE(paintLayer->graphicsLayerBackingForScrolling());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersPromotedOnStyleChange)
{
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

    setBodyInnerHTML(
        "<style>"
        "#scroller { overflow: scroll; height: 200px; width: 200px; background: rgba(255,255,255,0.5) local content-box; }"
        "#scrolled { height: 300px; }"
        "</style>"
        "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
    Element* scroller = document().getElementById("scroller");
    PaintLayer* paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    EXPECT_FALSE(paintLayer->needsCompositedScrolling());

    // Change the background to transparent
    scroller->setAttribute(HTMLNames::styleAttr, "background: white local content-box;");
    document().view()->updateAllLifecyclePhases();
    paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
    ASSERT_TRUE(paintLayer);
    EXPECT_TRUE(paintLayer->needsCompositedScrolling());
    EXPECT_TRUE(paintLayer->graphicsLayerBacking());
    ASSERT_TRUE(paintLayer->graphicsLayerBackingForScrolling());
    EXPECT_TRUE(paintLayer->graphicsLayerBackingForScrolling()->contentsOpaque());
}


}
