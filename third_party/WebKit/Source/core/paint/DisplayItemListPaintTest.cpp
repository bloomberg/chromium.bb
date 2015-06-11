// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/DeprecatedPaintLayerPainter.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ScopeRecorder.h"
#include "core/paint/SubtreeRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include <gtest/gtest.h>

namespace blink {

class DisplayItemListPaintTest : public RenderingTest {
public:
    DisplayItemListPaintTest()
        : m_layoutView(nullptr)
        , m_originalSlimmingPaintEnabled(RuntimeEnabledFeatures::slimmingPaintEnabled()) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView().layer()->graphicsLayerBacking()->displayItemList(); }
    const DisplayItems& newPaintListBeforeUpdate() { return rootDisplayItemList().m_newDisplayItems; }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    virtual void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(m_originalSlimmingPaintEnabled);
    }

    LayoutView* m_layoutView;
    bool m_originalSlimmingPaintEnabled;
};

class TestDisplayItem : public DisplayItem {
public:
    TestDisplayItem(const DisplayItemClientWrapper& client, Type type) : DisplayItem(client, type) { }

    virtual void replay(GraphicsContext&) override final { ASSERT_NOT_REACHED(); }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final { ASSERT_NOT_REACHED(); }
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(i, expected, actual) \
    String trace = String::format("%d: ", (int)i) + "Expected: " + (expected).asDebugString() + " Actual: " + (actual).asDebugString(); \
    SCOPED_TRACE(trace.utf8().data());
#else
#define TRACE_DISPLAY_ITEMS(i, expected, actual)
#endif

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...) \
    do { \
        EXPECT_EQ((size_t)expectedSize, actual.size()); \
        if (expectedSize != actual.size()) \
            break; \
        const TestDisplayItem expected[] = { __VA_ARGS__ }; \
        for (size_t index = 0; index < std::min<size_t>(actual.size(), expectedSize); index++) { \
            TRACE_DISPLAY_ITEMS(index, expected[index], actual[index]); \
            EXPECT_EQ(expected[index].client(), actual[index].client()); \
            EXPECT_EQ(expected[index].type(), actual[index].type()); \
        } \
    } while (false);

TEST_F(DisplayItemListPaintTest, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    LayoutView& layoutView = *document().layoutView();
    DeprecatedPaintLayer& rootLayer = *layoutView.layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), PaintBehaviorNormal, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div.focus();
    document().view()->updateLayoutAndStyleForPainting();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(layoutView.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(textInlineBox.displayItemClient()));
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 3,
        TestDisplayItem(layoutView, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret)); // New!
}

TEST_F(DisplayItemListPaintTest, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    LayoutView& layoutView = *document().layoutView();
    DeprecatedPaintLayer& rootLayer = *layoutView.layer();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();
    DisplayItemClient firstTextBoxDisplayItemClient = firstTextBox.displayItemClient();

    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), PaintBehaviorNormal, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(firstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateLayoutAndStyleForPainting();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(layoutView.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divBlock.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstTextBoxDisplayItemClient));
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 3,
        TestDisplayItem(layoutView, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(newFirstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(secondTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
}

} // namespace blink
