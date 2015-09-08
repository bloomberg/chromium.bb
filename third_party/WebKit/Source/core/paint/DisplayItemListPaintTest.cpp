// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DisplayItemListPaintTest.h"

#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/DeprecatedPaintLayerPainter.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ScopeRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

TEST_F(DisplayItemListPaintTest, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST_BASE(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div.focus();
    document().view()->updateAllLifecyclePhases();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(layoutView().displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(textInlineBox.displayItemClient()));
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST_BASE(rootDisplayItemList().displayItems(), 3,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret)); // New!
}

TEST_F(DisplayItemListPaintTest, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();
    DisplayItemClient firstTextBoxDisplayItemClient = firstTextBox.displayItemClient();

    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST_BASE(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(firstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateAllLifecyclePhases();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(layoutView().displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divBlock.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstTextBoxDisplayItemClient));
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST_BASE(rootDisplayItemList().displayItems(), 3,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(newFirstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(secondTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
}

TEST_F(DisplayItemListPaintTestForSlimmingPaintV2, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST_WITH_RED_FILL_IN_DEBUG(rootDisplayItemList().displayItems(), 4,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    div.focus();
    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST_WITH_RED_FILL_IN_DEBUG(rootDisplayItemList().displayItems(), 5,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret), // New!
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

TEST_F(DisplayItemListPaintTestForSlimmingPaintV2, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();

    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST_WITH_RED_FILL_IN_DEBUG(rootDisplayItemList().displayItems(), 4,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(firstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateAllLifecyclePhases();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST_WITH_RED_FILL_IN_DEBUG(rootDisplayItemList().displayItems(), 5,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(newFirstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(secondTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

TEST_F(DisplayItemListPaintTestForSlimmingPaintV2, CachedSubsequence)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='width: 100px; height: 100px; background-color: green'></div>"
        "</div>");
    document().view()->updateAllLifecyclePhases();

    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    EXPECT_DISPLAY_LIST_WITH_RED_FILL_IN_DEBUG(rootDisplayItemList().displayItems(), 11,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 100px; background-color: green");
    updateLifecyclePhasesToPaintForSlimmingPaintV2Clean();

    EXPECT_DISPLAY_LIST_WITH_CACHED_RED_FILL_IN_DEBUG(rootDisplayItemList().newDisplayItems(), 8,
        TestDisplayItem(layoutView(), DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST_WITH_RED_FILL_IN_DEBUG(rootDisplayItemList().displayItems(), 11,
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

} // namespace blink
