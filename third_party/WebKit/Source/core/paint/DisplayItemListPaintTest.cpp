// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DisplayItemListPaintTest.h"

#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintLayerPainter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

TEST_F(DisplayItemListPaintTest, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    PaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    GraphicsContext context(&rootDisplayItemList());
    PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(textInlineBox, foregroundType));

    div.focus();
    document().view()->updateAllLifecyclePhases();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(layoutView().displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(textInlineBox.displayItemClient()));
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 3,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret)); // New!
}

TEST_F(DisplayItemListPaintTest, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    PaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();
    DisplayItemClient firstTextBoxDisplayItemClient = firstTextBox.displayItemClient();

    GraphicsContext context(&rootDisplayItemList());
    PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(firstTextBox, foregroundType));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateAllLifecyclePhases();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(layoutView().displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divBlock.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstTextBoxDisplayItemClient));
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 3,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(newFirstTextBox, foregroundType),
        TestDisplayItem(secondTextBox, foregroundType));
}

TEST_F(DisplayItemListPaintTestForSlimmingPaintV2, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    PaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 4,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    div.focus();
    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 5,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret), // New!
        TestDisplayItem(rootLayer, endSubsequenceType));
}

TEST_F(DisplayItemListPaintTestForSlimmingPaintV2, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    PaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();

    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 4,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(firstTextBox, foregroundType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateAllLifecyclePhases();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 5,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(newFirstTextBox, foregroundType),
        TestDisplayItem(secondTextBox, foregroundType),
        TestDisplayItem(rootLayer, endSubsequenceType));
}

} // namespace blink
