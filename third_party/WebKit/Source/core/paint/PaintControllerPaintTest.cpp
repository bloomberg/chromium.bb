// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/PaintControllerPaintTest.h"

#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintLayerPainter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

INSTANTIATE_TEST_CASE_P(All, PaintControllerPaintTestForSlimmingPaintV1AndV2, ::testing::Bool());

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    PaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    if (RuntimeEnabledFeatures::slimmingPaintSynchronizedPaintingEnabled()) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 4,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(textInlineBox, foregroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        GraphicsContext context(rootPaintController());
        PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
        PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
        rootPaintController().commitNewDisplayItems();

        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 2,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(textInlineBox, foregroundType));
    }

    div.focus();
    document().view()->updateAllLifecyclePhases();

    if (RuntimeEnabledFeatures::slimmingPaintSynchronizedPaintingEnabled()) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 5,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(textInlineBox, foregroundType),
            TestDisplayItem(divLayoutObject, DisplayItem::Caret), // New!
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        GraphicsContext context(rootPaintController());
        PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
        PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
        rootPaintController().commitNewDisplayItems();

        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 3,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(textInlineBox, foregroundType),
            TestDisplayItem(divLayoutObject, DisplayItem::Caret)); // New!
    }
}

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    PaintLayer& rootLayer = *layoutView().layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();

    if (RuntimeEnabledFeatures::slimmingPaintSynchronizedPaintingEnabled()) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 4,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(firstTextBox, foregroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        GraphicsContext context(rootPaintController());
        PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
        PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
        rootPaintController().commitNewDisplayItems();

        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 2,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(firstTextBox, foregroundType));
    }

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateAllLifecyclePhases();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    if (RuntimeEnabledFeatures::slimmingPaintSynchronizedPaintingEnabled()) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 5,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(newFirstTextBox, foregroundType),
            TestDisplayItem(secondTextBox, foregroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        GraphicsContext context(rootPaintController());
        PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
        PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
        rootPaintController().commitNewDisplayItems();

        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 3,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(newFirstTextBox, foregroundType),
            TestDisplayItem(secondTextBox, foregroundType));
    }
}

} // namespace blink
