// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGInlineTextBoxPainter.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/DOMSelection.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/IntRectOutsets.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class SVGInlineTextBoxPainterTest : public RenderingTest {
public:
    const DrawingDisplayItem* getDrawingForSVGTextById(const char* elementName)
    {
        // Look up the inline text box that serves as the display item client for the painted text.
        LayoutSVGText* targetSVGText = toLayoutSVGText(
            document().getElementById(AtomicString(elementName))->layoutObject());
        LayoutSVGInlineText* targetInlineText = targetSVGText->descendantTextNodes()[0];
        const DisplayItemClient* targetClient = static_cast<const DisplayItemClient*>(targetInlineText->firstTextBox());

        // Find the appropriate drawing in the display item list.
        const DisplayItemList& displayItemList = rootPaintController().getDisplayItemList();
        for (size_t i = 0; i < displayItemList.size(); i++) {
            if (displayItemList[i].client() == *targetClient)
                return static_cast<const DrawingDisplayItem*>(&displayItemList[i]);
        }

        return nullptr;
    }

    void selectAllText()
    {
        Range* range = document().createRange();
        range->selectNode(document().documentElement());
        LocalDOMWindow* window = document().domWindow();
        DOMSelection* selection = window->getSelection();
        selection->removeAllRanges();
        selection->addRange(range);
    }

private:
    PaintController& rootPaintController()
    {
        return document().view()->layoutView()->layer()->graphicsLayerBacking()->getPaintController();
    }

    void SetUp() override
    {
        RenderingTest::SetUp();
        enableCompositing();
    }
};

static void assertTextDrawingEquals(const DrawingDisplayItem* drawingDisplayItem, const char* str)
{
    ASSERT_EQ(str, static_cast<const InlineTextBox*>(&drawingDisplayItem->client())->text());
}

const static int kAllowedPixelDelta = 8;

static void assertCullRectEquals(const DrawingDisplayItem* drawingDisplayItem, const IntRect& expectedRect)
{
    // Text metrics can vary slightly across platforms, so we allow for a small pixel difference.
    IntRect outerRect(expectedRect);
    int delta = kAllowedPixelDelta / 2;
    outerRect.expand(IntRectOutsets(delta, delta, delta, delta));
    IntRect innerRect(expectedRect);
    // Rect contains is inclusive of edge, so shrink by one extra pixel.
    int contractDelta = -delta - 1;
    innerRect.expand(IntRectOutsets(contractDelta, contractDelta, contractDelta, contractDelta));

    IntRect actualRect(IntRect(drawingDisplayItem->picture()->cullRect()));
    ASSERT_TRUE(outerRect.contains(actualRect) && !innerRect.contains(actualRect))
        << "Cull rect not approximately equal [expected=("
        << expectedRect.x() << "," << expectedRect.y() << " " << expectedRect.width() << "x" << expectedRect.height() << "), actual=("
        << actualRect.x() << "," << actualRect.y() << " " << actualRect.width() << "x" << actualRect.height() << ")].";
}

TEST_F(SVGInlineTextBoxPainterTest, TextCullRect_DefaultWritingMode)
{
    setBodyInnerHTML(
        "<svg width='400px' height='400px' font-family='Arial' font-size='30'>"
        "<text id='target' x='50' y='30'>x</text>"
        "</svg>");
    document().view()->updateAllLifecyclePhases();

    const DrawingDisplayItem* drawingDisplayItem = getDrawingForSVGTextById("target");
    assertTextDrawingEquals(drawingDisplayItem, "x");
    assertCullRectEquals(drawingDisplayItem, IntRect(50, 3, 15, 33));

    selectAllText();
    document().view()->updateAllLifecyclePhases();

    drawingDisplayItem = getDrawingForSVGTextById("target");
    assertTextDrawingEquals(drawingDisplayItem, "x");
    assertCullRectEquals(drawingDisplayItem, IntRect(50, 3, 15, 33));
}

TEST_F(SVGInlineTextBoxPainterTest, TextCullRect_WritingModeTopToBottom)
{
    setBodyInnerHTML(
        "<svg width='400px' height='400px' font-family='Arial' font-size='30'>"
        "<text id='target' x='50' y='30' writing-mode='tb'>x</text>"
        "</svg>");
    document().view()->updateAllLifecyclePhases();

    const DrawingDisplayItem* drawingDisplayItem = getDrawingForSVGTextById("target");
    assertTextDrawingEquals(drawingDisplayItem, "x");
    assertCullRectEquals(drawingDisplayItem, IntRect(33, 30, 34, 15));

    selectAllText();
    document().view()->updateAllLifecyclePhases();

    // The selection rect is one pixel taller due to sub-pixel difference
    // between the text bounds and selection bounds in combination with use of
    // enclosingIntRect() in SVGInlineTextBox::localSelectionRect().
    drawingDisplayItem = getDrawingForSVGTextById("target");
    assertTextDrawingEquals(drawingDisplayItem, "x");
    assertCullRectEquals(drawingDisplayItem, IntRect(33, 30, 34, 16));
}

} // namespace
} // namespace blink
