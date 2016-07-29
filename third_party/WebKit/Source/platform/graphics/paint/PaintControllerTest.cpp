// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class PaintControllerTestBase : public testing::Test {
public:
    PaintControllerTestBase()
        : m_paintController(PaintController::create()) { }

    IntRect visualRect(const PaintArtifact& paintArtifact, size_t index)
    {
        return paintArtifact.getDisplayItemList().visualRect(index);
    }

protected:
    PaintController& getPaintController() { return *m_paintController; }

    int numCachedNewItems() const { return m_paintController->m_numCachedNewItems; }

#if DCHECK_IS_ON()
    int numSequentialMatches() const { return m_paintController->m_numSequentialMatches; }
    int numOutOfOrderMatches() const { return m_paintController->m_numOutOfOrderMatches; }
    int numIndexedItems() const { return m_paintController->m_numIndexedItems; }
#endif

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

private:
    std::unique_ptr<PaintController> m_paintController;
    RuntimeEnabledFeatures::Backup m_featuresBackup;
};

const DisplayItem::Type foregroundDrawingType = static_cast<DisplayItem::Type>(DisplayItem::DrawingPaintPhaseFirst + 4);
const DisplayItem::Type backgroundDrawingType = DisplayItem::DrawingPaintPhaseFirst;
const DisplayItem::Type clipType = DisplayItem::ClipFirst;

class TestDisplayItem final : public DisplayItem {
public:
    TestDisplayItem(const FakeDisplayItemClient& client, Type type) : DisplayItem(client, type, sizeof(*this)) { }

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
            EXPECT_EQ(expected[index].getType(), actual[index].getType()); \
        } \
    } while (false);

void drawRect(GraphicsContext& context, const FakeDisplayItemClient& client, DisplayItem::Type type, const FloatRect& bounds)
{
    if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
        return;
    DrawingRecorder drawingRecorder(context, client, type, bounds);
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

void drawClippedRect(GraphicsContext& context, const FakeDisplayItemClient& client, DisplayItem::Type clipType, DisplayItem::Type drawingType, const FloatRect& bound)
{
    ClipRecorder clipRecorder(context, client, clipType, IntRect(1, 1, 9, 9));
    drawRect(context, client, drawingType, bound);
}

#if DCHECK_IS_ON()
// Tests using this class will be tested with under-invalidation-checking enabled and disabled.
class PaintControllerTest : public PaintControllerTestBase, public testing::WithParamInterface<bool> {
protected:
    void SetUp() override
    {
        if (GetParam())
            RuntimeEnabledFeatures::setSlimmingPaintUnderInvalidationCheckingEnabled(true);
    }
};

INSTANTIATE_TEST_CASE_P(All, PaintControllerTest, ::testing::Bool());
#define TEST_F_OR_P TEST_P
#else
// Under-invalidation checking is only available when DCHECK_IS_ON().
using PaintControllerTest = PaintControllerTestBase;
#define TEST_F_OR_P TEST_F
#endif

TEST_F_OR_P(PaintControllerTest, NestedRecorders)
{
    GraphicsContext context(getPaintController());

    FakeDisplayItemClient client("client");

    drawClippedRect(context, client, clipType, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(client, clipType),
        TestDisplayItem(client, backgroundDrawingType),
        TestDisplayItem(client, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F_OR_P(PaintControllerTest, UpdateBasic)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));

    EXPECT_EQ(0, numCachedNewItems());

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));

    EXPECT_EQ(2, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(2, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(1, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateSwapOrder)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient unaffected("unaffected");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    drawRect(context, unaffected, foregroundDrawingType, FloatRect(300, 300, 10, 10));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType),
        TestDisplayItem(unaffected, foregroundDrawingType));

    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    drawRect(context, unaffected, foregroundDrawingType, FloatRect(300, 300, 10, 10));

    EXPECT_EQ(6, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(5, numSequentialMatches()); // second, first foreground, unaffected
    EXPECT_EQ(1, numOutOfOrderMatches()); // first
    EXPECT_EQ(2, numIndexedItems()); // first
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType),
        TestDisplayItem(unaffected, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateSwapOrderWithInvalidation)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient unaffected("unaffected");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    drawRect(context, unaffected, foregroundDrawingType, FloatRect(300, 300, 10, 10));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType),
        TestDisplayItem(unaffected, foregroundDrawingType));

    first.setDisplayItemsUncached();
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    drawRect(context, unaffected, foregroundDrawingType, FloatRect(300, 300, 10, 10));

    EXPECT_EQ(4, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(4, numSequentialMatches()); // second, unaffected
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(2, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType),
        TestDisplayItem(unaffected, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateNewItemInMiddle)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient third("third");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, third, backgroundDrawingType, FloatRect(125, 100, 200, 50));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));

    EXPECT_EQ(2, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(2, numSequentialMatches()); // first, second
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(0, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateInvalidationWithPhases)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient third("third");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));

    EXPECT_EQ(4, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(4, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(2, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateAddFirstOverlap)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    first.setDisplayItemsUncached();
    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    EXPECT_EQ(0, numCachedNewItems());
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    first.setDisplayItemsUncached();
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));

    EXPECT_EQ(2, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(2, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(2, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateAddLastOverlap)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    first.setDisplayItemsUncached();
    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    EXPECT_EQ(0, numCachedNewItems());
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    first.setDisplayItemsUncached();
    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    EXPECT_EQ(0, numCachedNewItems());
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, UpdateClip)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    {
        ClipRecorder clipRecorder(context, first, clipType, IntRect(1, 1, 2, 2));
        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, clipType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, DisplayItem::clipTypeToEndClipType(clipType)));

    first.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));

    EXPECT_EQ(1, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(1, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(1, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    {
        ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, clipType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F_OR_P(PaintControllerTest, CachedDisplayItems)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(first));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(second));
    const SkPicture* firstPicture = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[0]).picture();
    const SkPicture* secondPicture = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture();

    first.setDisplayItemsUncached();
    EXPECT_FALSE(getPaintController().clientCacheIsValid(first));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(second));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    // The first display item should be updated.
    EXPECT_NE(firstPicture, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[0]).picture());
    // The second display item should be cached.
    if (!RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        EXPECT_EQ(secondPicture, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture());
    EXPECT_TRUE(getPaintController().clientCacheIsValid(first));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(second));

    getPaintController().invalidateAll();
    EXPECT_FALSE(getPaintController().clientCacheIsValid(first));
    EXPECT_FALSE(getPaintController().clientCacheIsValid(second));
}

TEST_F_OR_P(PaintControllerTest, ComplexUpdateSwapOrder)
{
    FakeDisplayItemClient container1("container1");
    FakeDisplayItemClient content1("content1");
    FakeDisplayItemClient container2("container2");
    FakeDisplayItemClient content2("content2");
    GraphicsContext context(getPaintController());

    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 8,
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    container1.setDisplayItemsUncached();
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 8,
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, CachedSubsequenceSwapOrder)
{
    FakeDisplayItemClient container1("container1");
    FakeDisplayItemClient content1("content1");
    FakeDisplayItemClient container2("container2");
    FakeDisplayItemClient content2("content2");
    GraphicsContext context(getPaintController());

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
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 12,
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
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        // When under-invalidation-checking is enabled, useCachedSubsequenceIfPossible is forced off,
        // and the client is expected to create the same painting as in the previous paint.
        EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
        {
            SubsequenceRecorder r(context, container2);
            drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
            drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
            drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
            drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
        }
        EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container1));
        {
            SubsequenceRecorder r(context, container1);
            drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
            drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
            drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
            drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
        }
    } else {
        EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
        EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container1));
    }

    EXPECT_EQ(12, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(1, numSequentialMatches());
    EXPECT_EQ(1, numOutOfOrderMatches());
    EXPECT_EQ(5, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 12,
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

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

TEST_F_OR_P(PaintControllerTest, OutOfOrderNoCrash)
{
    FakeDisplayItemClient client("client");
    GraphicsContext context(getPaintController());

    const DisplayItem::Type type1 = DisplayItem::DrawingFirst;
    const DisplayItem::Type type2 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 1);
    const DisplayItem::Type type3 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 2);
    const DisplayItem::Type type4 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 3);

    drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
    drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

    getPaintController().commitNewDisplayItems();

    drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
    drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

    getPaintController().commitNewDisplayItems();
}

TEST_F_OR_P(PaintControllerTest, CachedNestedSubsequenceUpdate)
{
    FakeDisplayItemClient container1("container1");
    FakeDisplayItemClient content1("content1");
    FakeDisplayItemClient container2("container2");
    FakeDisplayItemClient content2("content2");
    GraphicsContext context(getPaintController());

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
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 14,
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
    container1.setDisplayItemsUncached();

    // Container2 itself now becomes empty (but still has the 'content2' child),
    // and chooses not to output subsequence info.

    container2.setDisplayItemsUncached();
    content2.setDisplayItemsUncached();
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
        if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
            // When under-invalidation-checking is enabled, useCachedSubsequenceIfPossible is forced off,
            // and the client is expected to create the same painting as in the previous paint.
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content1));
            SubsequenceRecorder r(context, content1);
            drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
            drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        } else {
            EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content1));
        }
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }

    EXPECT_EQ(4, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(1, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(2, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 10,
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

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

TEST_F_OR_P(PaintControllerTest, SkipCache)
{
    FakeDisplayItemClient multicol("multicol");
    FakeDisplayItemClient content("content");
    GraphicsContext context(getPaintController());

    FloatRect rect1(100, 100, 50, 50);
    FloatRect rect2(150, 100, 50, 50);
    FloatRect rect3(200, 100, 50, 50);

    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));

    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect1);
    drawRect(context, content, foregroundDrawingType, rect2);
    getPaintController().endSkippingCache();

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    RefPtr<const SkPicture> picture1 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture();
    RefPtr<const SkPicture> picture2 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[2]).picture();
    EXPECT_NE(picture1, picture2);

    // Draw again with nothing invalidated.
    EXPECT_TRUE(getPaintController().clientCacheIsValid(multicol));
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));

    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect1);
    drawRect(context, content, foregroundDrawingType, rect2);
    getPaintController().endSkippingCache();

    EXPECT_EQ(1, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(1, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(0, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[2]).picture());

    // Now the multicol becomes 3 columns and repaints.
    multicol.setDisplayItemsUncached();
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 100, 100, 100));

    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect1);
    drawRect(context, content, foregroundDrawingType, rect2);
    drawRect(context, content, foregroundDrawingType, rect3);
    getPaintController().endSkippingCache();

    // We should repaint everything on invalidation of the scope container.
    EXPECT_DISPLAY_LIST(getPaintController().newDisplayItemList(), 4,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(getPaintController().newDisplayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(getPaintController().newDisplayItemList()[2]).picture());

    getPaintController().commitNewDisplayItems();
}

TEST_F_OR_P(PaintControllerTest, PartialSkipCache)
{
    FakeDisplayItemClient content("content");
    GraphicsContext context(getPaintController());

    FloatRect rect1(100, 100, 50, 50);
    FloatRect rect2(150, 100, 50, 50);
    FloatRect rect3(200, 100, 50, 50);

    drawRect(context, content, backgroundDrawingType, rect1);
    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect2);
    getPaintController().endSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect3);

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(content, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    RefPtr<const SkPicture> picture0 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[0]).picture();
    RefPtr<const SkPicture> picture1 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture();
    RefPtr<const SkPicture> picture2 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[2]).picture();
    EXPECT_NE(picture1, picture2);

    // Content's cache is invalid because it has display items skipped cache.
    EXPECT_FALSE(getPaintController().clientCacheIsValid(content));
    EXPECT_EQ(PaintInvalidationFull, content.getPaintInvalidationReason());

    // Draw again with nothing invalidated.
    drawRect(context, content, backgroundDrawingType, rect1);
    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect2);
    getPaintController().endSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect3);

    EXPECT_EQ(0, numCachedNewItems());
#if DCHECK_IS_ON()
    EXPECT_EQ(0, numSequentialMatches());
    EXPECT_EQ(0, numOutOfOrderMatches());
    EXPECT_EQ(0, numIndexedItems());
#endif

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(content, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture0, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[0]).picture());
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[2]).picture());
}

TEST_F_OR_P(PaintControllerTest, OptimizeNoopPairs)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient third("third");

    GraphicsContext context(getPaintController());
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipPathRecorder clipRecorder(context, second, Path());
        drawRect(context, second, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    getPaintController().commitNewDisplayItems();
    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 5,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::BeginClipPath),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::EndClipPath),
        TestDisplayItem(third, backgroundDrawingType));

    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
        // Do not draw anything for second.
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    getPaintController().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
        {
            ClipPathRecorder clipPathRecorder(context, second, Path());
            // Do not draw anything for second.
        }
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    getPaintController().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));
}

TEST_F_OR_P(PaintControllerTest, SmallPaintControllerHasOnePaintChunk)
{
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    FakeDisplayItemClient client("test client");

    GraphicsContext context(getPaintController());
    drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    getPaintController().commitNewDisplayItems();
    const auto& paintChunks = getPaintController().paintChunks();
    ASSERT_EQ(1u, paintChunks.size());
    EXPECT_EQ(0u, paintChunks[0].beginIndex);
    EXPECT_EQ(1u, paintChunks[0].endIndex);
}

#define EXPECT_RECT_EQ(expected, actual) \
    do { \
        const IntRect& actualRect = actual; \
        EXPECT_EQ(expected.x(), actualRect.x()); \
        EXPECT_EQ(expected.y(), actualRect.y()); \
        EXPECT_EQ(expected.width(), actualRect.width()); \
        EXPECT_EQ(expected.height(), actualRect.height()); \
    } while (false)

TEST_F_OR_P(PaintControllerTest, PaintArtifactWithVisualRects)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

    GraphicsContext context(getPaintController());
    drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    getPaintController().commitNewDisplayItems(LayoutSize(20, 30));
    const auto& paintArtifact = getPaintController().paintArtifact();
    ASSERT_EQ(1u, paintArtifact.getDisplayItemList().size());
    EXPECT_RECT_EQ(IntRect(-20, -30, 200, 100), visualRect(paintArtifact, 0));
}

void drawPath(GraphicsContext& context, DisplayItemClient& client, DisplayItem::Type type, unsigned count)
{
    if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
        return;

    DrawingRecorder drawingRecorder(context, client, type, FloatRect(0, 0, 100, 100));
    SkPath path;
    path.moveTo(0, 0);
    path.lineTo(0, 100);
    path.lineTo(50, 50);
    path.lineTo(100, 100);
    path.lineTo(100, 0);
    path.close();
    SkPaint paint;
    paint.setAntiAlias(true);
    for (unsigned i = 0; i < count; i++)
        context.drawPath(path, paint);
}

TEST_F_OR_P(PaintControllerTest, IsSuitableForGpuRasterizationSinglePath)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());
    drawPath(context, client, backgroundDrawingType, 1);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_TRUE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F_OR_P(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPaths)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());

    drawPath(context, client, backgroundDrawingType, 50);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F_OR_P(PaintControllerTest, IsNotSuitableForGpuRasterizationMultiplePicturesSinglePathEach)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());
    getPaintController().beginSkippingCache();

    for (int i = 0; i < 50; ++i)
        drawPath(context, client, backgroundDrawingType, 50);

    getPaintController().endSkippingCache();
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F_OR_P(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPathsTwoPaints)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }

    client.setDisplayItemsUncached();

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }
}

TEST_F_OR_P(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPathsCached)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }
}

TEST_F(PaintControllerTestBase, IsNotSuitableForGpuRasterizationSinglePictureManyPathsCachedSubsequence)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    FakeDisplayItemClient container("container", LayoutRect(0, 0, 200, 100));

    GraphicsContext context(getPaintController());
    {
        SubsequenceRecorder subsequenceRecorder(context, container);
        drawPath(context, client, backgroundDrawingType, 50);
    }
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());

    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

// Temporarily disabled (pref regressions due to GPU veto stickiness: http://crbug.com/603969).
TEST_F_OR_P(PaintControllerTest, DISABLED_IsNotSuitableForGpuRasterizationConcaveClipPath)
{
    Path path;
    path.addLineTo(FloatPoint(50, 50));
    path.addLineTo(FloatPoint(100, 0));
    path.addLineTo(FloatPoint(50, 100));
    path.closeSubpath();

    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());

    // Run twice for empty/non-empty m_currentPaintArtifact coverage.
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 50; ++j)
            getPaintController().createAndAppend<BeginClipPathDisplayItem>(client, path);
        drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));
        for (int j = 0; j < 50; ++j)
            getPaintController().createAndAppend<EndClipPathDisplayItem>(client);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }
}

// Under-invalidation checking is only available when DCHECK_IS_ON().
// Death tests don't work properly on Android.
#if DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST) && !OS(ANDROID)

class PaintControllerUnderInvalidationTest : public PaintControllerTestBase {
protected:
    void SetUp() override
    {
        PaintControllerTestBase::SetUp();
        RuntimeEnabledFeatures::setSlimmingPaintUnderInvalidationCheckingEnabled(true);
    }

    void testChangeDrawing()
    {
        FakeDisplayItemClient first("first");
        GraphicsContext context(getPaintController());

        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        getPaintController().commitNewDisplayItems();
        drawRect(context, first, backgroundDrawingType, FloatRect(200, 200, 300, 300));
        drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        getPaintController().commitNewDisplayItems();
    }

    void testMoreDrawing()
    {
        FakeDisplayItemClient first("first");
        GraphicsContext context(getPaintController());

        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        getPaintController().commitNewDisplayItems();
        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        getPaintController().commitNewDisplayItems();
    }

    void testLessDrawing()
    {
        FakeDisplayItemClient first("first");
        GraphicsContext context(getPaintController());

        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        getPaintController().commitNewDisplayItems();
        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        getPaintController().commitNewDisplayItems();
    }

    void testNoopPairsInSubsequence()
    {
        FakeDisplayItemClient container("container");
        GraphicsContext context(getPaintController());

        {
            SubsequenceRecorder r(context, container);
            drawRect(context, container, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        }
        getPaintController().commitNewDisplayItems();

        EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
        {
            // Generate some no-op pairs which should not affect under-invalidation checking.
            ClipRecorder r1(context, container, clipType, IntRect(1, 1, 9, 9));
            ClipRecorder r2(context, container, clipType, IntRect(1, 1, 2, 2));
            ClipRecorder r3(context, container, clipType, IntRect(1, 1, 3, 3));
            ClipPathRecorder r4(context, container, Path());
        }
        {
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
            SubsequenceRecorder r(context, container);
            drawRect(context, container, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        }
        getPaintController().commitNewDisplayItems();

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
        DisplayItemClient::endShouldKeepAliveAllClients();
#endif
    }

    void testChangeDrawingInSubsequence()
    {
        FakeDisplayItemClient first("first");
        GraphicsContext context(getPaintController());
        {
            SubsequenceRecorder r(context, first);
            drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
            drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();
        {
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, first));
            SubsequenceRecorder r(context, first);
            drawRect(context, first, backgroundDrawingType, FloatRect(200, 200, 300, 300));
            drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();
    }

    void testMoreDrawingInSubsequence()
    {
        FakeDisplayItemClient first("first");
        GraphicsContext context(getPaintController());

        {
            SubsequenceRecorder r(context, first);
            drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();

        {
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, first));
            SubsequenceRecorder r(context, first);
            drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
            drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();
    }

    void testLessDrawingInSubsequence()
    {
        FakeDisplayItemClient first("first");
        GraphicsContext context(getPaintController());

        {
            SubsequenceRecorder r(context, first);
            drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
            drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();

        {
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, first));
            SubsequenceRecorder r(context, first);
            drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();
    }

    void testChangeNonCacheableInSubsequence()
    {
        FakeDisplayItemClient container("container");
        FakeDisplayItemClient content("content");
        GraphicsContext context(getPaintController());

        {
            SubsequenceRecorder r(context, container);
            {
                ClipPathRecorder clipPathRecorder(context, container, Path());
            }
            ClipRecorder clip(context, container, clipType, IntRect(1, 1, 9, 9));
            drawRect(context, content, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();

        {
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
            SubsequenceRecorder r(context, container);
            {
                ClipPathRecorder clipPathRecorder(context, container, Path());
            }
            ClipRecorder clip(context, container, clipType, IntRect(1, 1, 30, 30));
            drawRect(context, content, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();
    }

    void testInvalidationInSubsequence()
    {
        FakeDisplayItemClient container("container");
        FakeDisplayItemClient content("content");
        GraphicsContext context(getPaintController());

        {
            SubsequenceRecorder r(context, container);
            drawRect(context, content, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();

        content.setDisplayItemsUncached();
        // Leave container not invalidated.
        {
            EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
            SubsequenceRecorder r(context, content);
            drawRect(context, content, backgroundDrawingType, FloatRect(100, 100, 300, 300));
        }
        getPaintController().commitNewDisplayItems();
    }

    // TODO(wangxianzhu): Add under-invalidation checking test in case of compositing item folding.
};

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawing)
{
    EXPECT_DEATH(testChangeDrawing(), "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawing)
{
    EXPECT_DEATH(testMoreDrawing(), "Can't find cached display item");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawing)
{
    // We don't detect under-invalidation in this case, and PaintController can also handle the case gracefully.
    // However, less-drawing at a time often means more-drawing at another time so eventually we'll detect
    // such under-invalidations.
    testLessDrawing();
}

TEST_F(PaintControllerUnderInvalidationTest, NoopPairsInSubsequence)
{
    // This should not die.
    testNoopPairsInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawingInSubsequence)
{
    EXPECT_DEATH(testChangeDrawingInSubsequence(), "\"\\(In cached subsequence of first\\)\" under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawingInSubsequence)
{
    EXPECT_DEATH(testMoreDrawingInSubsequence(), "\"\\(In cached subsequence of first\\)\" under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawingInSubsequence)
{
    EXPECT_DEATH(testLessDrawingInSubsequence(), "\"\\(In cached subsequence of first\\)\" under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeNonCacheableInSubsequence)
{
    EXPECT_DEATH(testChangeNonCacheableInSubsequence(), "\"\\(In cached subsequence of container\\)\" under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, InvalidationInSubsequence)
{
    EXPECT_DEATH(testInvalidationInSubsequence(), "\"\\(In cached subsequence of container\\)\" under-invalidation of PaintLayer: invalidated in cached subsequence");
}

#endif // DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST) && !OS(ANDROID)

} // namespace blink
