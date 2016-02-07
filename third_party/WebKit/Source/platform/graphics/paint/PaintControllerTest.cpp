// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PaintControllerTest : public ::testing::Test {
public:
    PaintControllerTest()
        : m_paintController(PaintController::create())
        , m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled()) { }

protected:
    PaintController& paintController() { return *m_paintController; }

private:
    void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_originalSlimmingPaintV2Enabled);
    }

    OwnPtr<PaintController> m_paintController;
    bool m_originalSlimmingPaintV2Enabled;
};

const DisplayItem::Type foregroundDrawingType = static_cast<DisplayItem::Type>(DisplayItem::DrawingPaintPhaseFirst + 4);
const DisplayItem::Type backgroundDrawingType = DisplayItem::DrawingPaintPhaseFirst;
const DisplayItem::Type clipType = DisplayItem::ClipFirst;

class TestDisplayItemClient : public DisplayItemClient {
public:
    TestDisplayItemClient(const String& name)
        : m_name(name)
    { }

    String debugName() const final { return m_name; }
    LayoutRect visualRect() const override { return LayoutRect(); }

private:
    String m_name;
};

class TestDisplayItem final : public DisplayItem {
public:
    TestDisplayItem(const TestDisplayItemClient& client, Type type) : DisplayItem(client, type, sizeof(*this)) { }

    void replay(GraphicsContext&) const final { ASSERT_NOT_REACHED(); }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final { ASSERT_NOT_REACHED(); }
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

void drawRect(GraphicsContext& context, const TestDisplayItemClient& client, DisplayItem::Type type, const FloatRect& bounds)
{
    if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
        return;
    DrawingRecorder drawingRecorder(context, client, type, bounds);
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

void drawClippedRect(GraphicsContext& context, const TestDisplayItemClient& client, DisplayItem::Type clipType, DisplayItem::Type drawingType, const FloatRect& bound)
{
    ClipRecorder clipRecorder(context, client, clipType, LayoutRect(1, 1, 9, 9));
    drawRect(context, client, drawingType, bound);
}

TEST_F(PaintControllerTest, NestedRecorders)
{
    GraphicsContext context(paintController());

    TestDisplayItemClient client("client");

    drawClippedRect(context, client, clipType, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(client, clipType),
        TestDisplayItem(client, backgroundDrawingType),
        TestDisplayItem(client, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F(PaintControllerTest, UpdateBasic)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(paintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateSwapOrder)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient unaffected("unaffected");
    GraphicsContext context(paintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType));

    paintController().invalidate(second);
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateNewItemInMiddle)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient third("third");
    GraphicsContext context(paintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, third, backgroundDrawingType, FloatRect(125, 100, 200, 50));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateInvalidationWithPhases)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient third("third");
    GraphicsContext context(paintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));

    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateAddFirstOverlap)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(paintController());

    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    paintController().invalidate(first);
    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    paintController().invalidate(first);
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateAddLastOverlap)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(paintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    paintController().invalidate(first);
    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    paintController().invalidate(first);
    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateClip)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(paintController());

    {
        ClipRecorder clipRecorder(context, first, clipType, LayoutRect(1, 1, 2, 2));
        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 4,
        TestDisplayItem(first, clipType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, DisplayItem::clipTypeToEndClipType(clipType)));

    paintController().invalidate(first);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    {
        ClipRecorder clipRecorder(context, second, clipType, LayoutRect(1, 1, 2, 2));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, clipType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F(PaintControllerTest, CachedDisplayItems)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(paintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    EXPECT_TRUE(paintController().clientCacheIsValid(first));
    EXPECT_TRUE(paintController().clientCacheIsValid(second));
    const SkPicture* firstPicture = static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[0]).picture();
    const SkPicture* secondPicture = static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[1]).picture();

    paintController().invalidate(first);
    EXPECT_FALSE(paintController().clientCacheIsValid(first));
    EXPECT_TRUE(paintController().clientCacheIsValid(second));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    // The first display item should be updated.
    EXPECT_NE(firstPicture, static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[0]).picture());
    // The second display item should be cached.
    EXPECT_EQ(secondPicture, static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[1]).picture());
    EXPECT_TRUE(paintController().clientCacheIsValid(first));
    EXPECT_TRUE(paintController().clientCacheIsValid(second));

    paintController().invalidateAll();
    EXPECT_FALSE(paintController().clientCacheIsValid(first));
    EXPECT_FALSE(paintController().clientCacheIsValid(second));
}

TEST_F(PaintControllerTest, ComplexUpdateSwapOrder)
{
    TestDisplayItemClient container1("container1");
    TestDisplayItemClient content1("content1");
    TestDisplayItemClient container2("container2");
    TestDisplayItemClient content2("content2");
    GraphicsContext context(paintController());

    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 8,
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    paintController().invalidate(container1);
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 8,
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType));
}

TEST_F(PaintControllerTest, CachedSubsequenceSwapOrder)
{
    TestDisplayItemClient container1("container1");
    TestDisplayItemClient content1("content1");
    TestDisplayItemClient container2("container2");
    TestDisplayItemClient content2("content2");
    GraphicsContext context(paintController());

    {
        SubsequenceRecorder r(context, container1);
        drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
        drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    {
        SubsequenceRecorder r(context, container2);
        drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
        drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
        drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
        drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    }
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 12,
        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence),

        TestDisplayItem(container2, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container2, DisplayItem::EndSubsequence));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container1));

    EXPECT_DISPLAY_LIST(paintController().newDisplayItemList(), 2,
        TestDisplayItem(container2, DisplayItem::CachedSubsequence),
        TestDisplayItem(container1, DisplayItem::CachedSubsequence));

    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 12,
        TestDisplayItem(container2, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container2, DisplayItem::EndSubsequence),

        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence));
}

TEST_F(PaintControllerTest, OutOfOrderNoCrash)
{
    TestDisplayItemClient client("client");
    GraphicsContext context(paintController());

    const DisplayItem::Type type1 = DisplayItem::DrawingFirst;
    const DisplayItem::Type type2 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 1);
    const DisplayItem::Type type3 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 2);
    const DisplayItem::Type type4 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 3);

    drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
    drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

    paintController().commitNewDisplayItems();

    drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
    drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

    paintController().commitNewDisplayItems();
}

TEST_F(PaintControllerTest, CachedNestedSubsequenceUpdate)
{
    TestDisplayItemClient container1("container1");
    TestDisplayItemClient content1("content1");
    TestDisplayItemClient container2("container2");
    TestDisplayItemClient content2("content2");
    GraphicsContext context(paintController());

    {
        SubsequenceRecorder r(context, container1);
        drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        {
            SubsequenceRecorder r(context, content1);
            drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
            drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        }
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    {
        SubsequenceRecorder r(context, container2);
        drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
        {
            SubsequenceRecorder r(context, content2);
            drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
        }
    }
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 14,
        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, DisplayItem::Subsequence),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(content1, DisplayItem::EndSubsequence),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence),

        TestDisplayItem(container2, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, DisplayItem::Subsequence),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, DisplayItem::EndSubsequence),
        TestDisplayItem(container2, DisplayItem::EndSubsequence));

    // Invalidate container1 but not content1.
    paintController().invalidate(container1);

    // Container2 itself now becomes empty (but still has the 'content2' child),
    // and chooses not to output subsequence info.

    paintController().invalidate(container2);
    paintController().invalidate(content2);
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content2));
    // Content2 now outputs foreground only.
    {
        SubsequenceRecorder r(context, content2);
        drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    }
    // Repaint container1 with foreground only.
    {
        EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container1));
        SubsequenceRecorder r(context, container1);
        // Use cached subsequence of content1.
        EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content1));
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    EXPECT_DISPLAY_LIST(paintController().newDisplayItemList(), 7,
        TestDisplayItem(content2, DisplayItem::Subsequence),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(content2, DisplayItem::EndSubsequence),
        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(content1, DisplayItem::CachedSubsequence),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence));

    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 10,
        TestDisplayItem(content2, DisplayItem::Subsequence),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(content2, DisplayItem::EndSubsequence),

        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(content1, DisplayItem::Subsequence),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(content1, DisplayItem::EndSubsequence),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence));
}

TEST_F(PaintControllerTest, Scope)
{
    TestDisplayItemClient multicol("multicol");
    TestDisplayItemClient content("content");
    GraphicsContext context(paintController());

    FloatRect rect1(100, 100, 50, 50);
    FloatRect rect2(150, 100, 50, 50);
    FloatRect rect3(200, 100, 50, 50);

    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));

    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect1);
    paintController().endScope();

    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect2);
    paintController().endScope();
    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    RefPtr<const SkPicture> picture1 = static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[1]).picture();
    RefPtr<const SkPicture> picture2 = static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[2]).picture();
    EXPECT_NE(picture1, picture2);

    // Draw again with nothing invalidated.
    EXPECT_TRUE(paintController().clientCacheIsValid(multicol));
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect1);
    paintController().endScope();

    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect2);
    paintController().endScope();

    EXPECT_DISPLAY_LIST(paintController().newDisplayItemList(), 3,
        TestDisplayItem(multicol, DisplayItem::drawingTypeToCachedDrawingType(backgroundDrawingType)),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));

    paintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(paintController().displayItemList()[2]).picture());

    // Now the multicol becomes 3 columns and repaints.
    paintController().invalidate(multicol);
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 100, 100, 100));

    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect1);
    paintController().endScope();

    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect2);
    paintController().endScope();

    paintController().beginScope();
    drawRect(context, content, foregroundDrawingType, rect3);
    paintController().endScope();

    // We should repaint everything on invalidation of the scope container.
    EXPECT_DISPLAY_LIST(paintController().newDisplayItemList(), 4,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(paintController().newDisplayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(paintController().newDisplayItemList()[2]).picture());

    paintController().commitNewDisplayItems();
}

TEST_F(PaintControllerTest, OptimizeNoopPairs)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient third("third");

    GraphicsContext context(paintController());
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipPathRecorder clipRecorder(context, second, Path());
        drawRect(context, second, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    paintController().commitNewDisplayItems();
    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 5,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::BeginClipPath),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::EndClipPath),
        TestDisplayItem(third, backgroundDrawingType));

    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, LayoutRect(1, 1, 2, 2));
        // Do not draw anything for second.
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    paintController().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));

    paintController().invalidate(second);
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, LayoutRect(1, 1, 2, 2));
        {
            ClipPathRecorder clipPathRecorder(context, second, Path());
            // Do not draw anything for second.
        }
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    paintController().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(paintController().displayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));
}

TEST_F(PaintControllerTest, SmallPaintControllerHasOnePaintChunk)
{
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    TestDisplayItemClient client("test client");

    GraphicsContext context(paintController());
    drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    paintController().commitNewDisplayItems();
    const auto& paintChunks = paintController().paintChunks();
    ASSERT_EQ(1u, paintChunks.size());
    EXPECT_EQ(0u, paintChunks[0].beginIndex);
    EXPECT_EQ(1u, paintChunks[0].endIndex);
}

} // namespace blink
