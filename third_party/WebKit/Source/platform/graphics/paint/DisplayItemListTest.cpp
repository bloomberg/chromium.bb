// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include <gtest/gtest.h>

namespace blink {

class DisplayItemListTest : public ::testing::Test {
public:
    DisplayItemListTest()
        : m_originalSlimmingPaintEnabled(RuntimeEnabledFeatures::slimmingPaintEnabled()) { }

protected:
    DisplayItemList& displayItemList() { return m_displayItemList; }
    const DisplayItems& newPaintListBeforeUpdate() { return displayItemList().m_newDisplayItems; }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);
    }
    virtual void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(m_originalSlimmingPaintEnabled);
    }

    DisplayItemList m_displayItemList;
    bool m_originalSlimmingPaintEnabled;
};

const DisplayItem::Type foregroundDrawingType = static_cast<DisplayItem::Type>(DisplayItem::DrawingPaintPhaseFirst + 4);
const DisplayItem::Type backgroundDrawingType = DisplayItem::BoxDecorationBackground;
const DisplayItem::Type clipType = DisplayItem::ClipFirst;

class TestDisplayItemClient {
public:
    TestDisplayItemClient(const String& name)
        : m_name(name)
    { }

    DisplayItemClient displayItemClient() const { return toDisplayItemClient(this); }
    String debugName() const { return m_name; }

private:
    String m_name;
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

void drawRect(GraphicsContext& context, const TestDisplayItemClient& client, DisplayItem::Type type, const FloatRect& bound)
{
    DrawingRecorder drawingRecorder(context, client, type, bound);
    if (drawingRecorder.canUseCachedDrawing())
        return;
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

void drawClippedRect(GraphicsContext& context, const TestDisplayItemClient& client, DisplayItem::Type clipType, DisplayItem::Type drawingType, const FloatRect& bound)
{
    ClipRecorder clipRecorder(context, client, clipType, LayoutRect(1, 1, 9, 9));
    drawRect(context, client, drawingType, bound);
}

TEST_F(DisplayItemListTest, NestedRecorders)
{
    GraphicsContext context(&displayItemList());

    TestDisplayItemClient client("client");

    drawClippedRect(context, client, clipType, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(client, clipType),
        TestDisplayItem(client, backgroundDrawingType),
        TestDisplayItem(client, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F(DisplayItemListTest, UpdateBasic)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(&displayItemList());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F(DisplayItemListTest, UpdateSwapOrder)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient unaffected("unaffected");
    GraphicsContext context(&displayItemList());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType));

    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType));
}

TEST_F(DisplayItemListTest, UpdateNewItemInMiddle)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient third("third");
    GraphicsContext context(&displayItemList());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, third, backgroundDrawingType, FloatRect(125, 100, 200, 50));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
}

TEST_F(DisplayItemListTest, UpdateInvalidationWithPhases)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient third("third");
    GraphicsContext context(&displayItemList());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));

    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));
}

TEST_F(DisplayItemListTest, UpdateAddFirstOverlap)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(&displayItemList());

    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    displayItemList().invalidate(first.displayItemClient());
    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    displayItemList().invalidate(first.displayItemClient());
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));
}

TEST_F(DisplayItemListTest, UpdateAddLastOverlap)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(&displayItemList());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    displayItemList().invalidate(first.displayItemClient());
    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    displayItemList().invalidate(first.displayItemClient());
    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F(DisplayItemListTest, UpdateClip)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(&displayItemList());

    {
        ClipRecorder clipRecorder(context, first, clipType, LayoutRect(1, 1, 2, 2));
        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 4,
        TestDisplayItem(first, clipType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, DisplayItem::clipTypeToEndClipType(clipType)));

    displayItemList().invalidate(first.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    {
        ClipRecorder clipRecorder(context, second, clipType, LayoutRect(1, 1, 2, 2));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, clipType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F(DisplayItemListTest, CachedDisplayItems)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    GraphicsContext context(&displayItemList());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    EXPECT_TRUE(displayItemList().clientCacheIsValid(first.displayItemClient()));
    EXPECT_TRUE(displayItemList().clientCacheIsValid(second.displayItemClient()));
    const SkPicture* firstPicture = displayItemList().displayItems()[0].picture();
    const SkPicture* secondPicture = displayItemList().displayItems()[1].picture();

    displayItemList().invalidate(first.displayItemClient());
    EXPECT_FALSE(displayItemList().clientCacheIsValid(first.displayItemClient()));
    EXPECT_TRUE(displayItemList().clientCacheIsValid(second.displayItemClient()));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    // The first display item should be updated.
    EXPECT_NE(firstPicture, displayItemList().displayItems()[0].picture());
    // The second display item should be cached.
    EXPECT_EQ(secondPicture, displayItemList().displayItems()[1].picture());
    EXPECT_TRUE(displayItemList().clientCacheIsValid(first.displayItemClient()));
    EXPECT_TRUE(displayItemList().clientCacheIsValid(second.displayItemClient()));

    displayItemList().invalidateAll();
    EXPECT_FALSE(displayItemList().clientCacheIsValid(first.displayItemClient()));
    EXPECT_FALSE(displayItemList().clientCacheIsValid(second.displayItemClient()));
}

TEST_F(DisplayItemListTest, ComplexUpdateSwapOrder)
{
    TestDisplayItemClient container1("container1");
    TestDisplayItemClient content1("content1");
    TestDisplayItemClient container2("container2");
    TestDisplayItemClient content2("content2");
    GraphicsContext context(&displayItemList());

    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 8,
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    displayItemList().invalidate(container1.displayItemClient());
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 8,
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType));
}

// Enable this when cached subtree flags are ready.
#if 0
TEST_F(DisplayItemListTest, CachedSubtreeSwapOrder)
{
    TestDisplayItemClient container1("container1");
    TestDisplayItemClient content1("content1");
    TestDisplayItemClient container2("container2");
    TestDisplayItemClient content2("content2");
    GraphicsContext context(nullptr, &displayItemList());

    {
        SubtreeRecorder r(context, container1, backgroundDrawingType);
        r.begin();
        drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    }
    {
        SubtreeRecorder r(context, container1, foregroundDrawingType);
        r.begin();
        drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    {
        SubtreeRecorder r(context, container2, backgroundDrawingType);
        r.begin();
        drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
        drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    }
    {
        SubtreeRecorder r(context, container2, foregroundDrawingType);
        r.begin();
        drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
        drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    }
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 16,
        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(backgroundDrawingType)),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(backgroundDrawingType)),

        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(foregroundDrawingType)),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(foregroundDrawingType)),

        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(backgroundDrawingType)),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(backgroundDrawingType)),

        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(foregroundDrawingType)),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(foregroundDrawingType)));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2,
    // and at the same time container2 is scrolled out of viewport and content2 is invalidated.
    displayItemList().invalidate(content2.displayItemClient());
    {
        SubtreeRecorder r(context, container2, backgroundDrawingType);
    }
    EXPECT_EQ((size_t)1, newPaintListBeforeUpdate().size());
    EXPECT_TRUE(newPaintListBeforeUpdate().last()->isSubtreeCached());
    {
        SubtreeRecorder r(context, container2, foregroundDrawingType);
    }
    EXPECT_EQ((size_t)2, newPaintListBeforeUpdate().size());
    EXPECT_TRUE(newPaintListBeforeUpdate().last()->isSubtreeCached());
    {
        SubtreeRecorder r(context, container1, backgroundDrawingType);
        r.begin();
        drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    }
    {
        SubtreeRecorder r(context, container1, foregroundDrawingType);
        r.begin();
        drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 14,
        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(backgroundDrawingType)),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(backgroundDrawingType)),

        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(foregroundDrawingType)),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(foregroundDrawingType)),

        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(backgroundDrawingType)),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(backgroundDrawingType)),

        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(foregroundDrawingType)),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(foregroundDrawingType)));
}
#endif

static bool isDrawing(const DisplayItems::ItemHandle& item)
{
    return DisplayItem::isDrawingType(item.type());
}

static bool isCached(const DisplayItems::ItemHandle& item)
{
    return DisplayItem::isCachedType(item.type());
}

TEST_F(DisplayItemListTest, Scope)
{
    TestDisplayItemClient multicol("multicol");
    TestDisplayItemClient content("content");
    GraphicsContext context(&displayItemList());

    FloatRect rect1(100, 100, 50, 50);
    FloatRect rect2(150, 100, 50, 50);
    FloatRect rect3(200, 100, 50, 50);

    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));

    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect1);
    displayItemList().endScope(multicol.displayItemClient());

    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect2);
    displayItemList().endScope(multicol.displayItemClient());
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    RefPtr<const SkPicture> picture1 = displayItemList().displayItems()[1].picture();
    RefPtr<const SkPicture> picture2 = displayItemList().displayItems()[2].picture();
    EXPECT_NE(picture1, picture2);

    // Draw again with nothing invalidated.
    EXPECT_TRUE(displayItemList().clientCacheIsValid(multicol.displayItemClient()));
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect1);
    displayItemList().endScope(multicol.displayItemClient());

    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect2);
    displayItemList().endScope(multicol.displayItemClient());

    EXPECT_TRUE(isCached(newPaintListBeforeUpdate()[0]));
    EXPECT_TRUE(isDrawing(newPaintListBeforeUpdate()[1]));
    EXPECT_TRUE(isDrawing(newPaintListBeforeUpdate()[2]));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, displayItemList().displayItems()[1].picture());
    EXPECT_NE(picture2, displayItemList().displayItems()[2].picture());

    // Now the multicol becomes 3 columns and repaints.
    displayItemList().invalidate(multicol.displayItemClient());
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 100, 100, 100));

    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect1);
    displayItemList().endScope(multicol.displayItemClient());

    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect2);
    displayItemList().endScope(multicol.displayItemClient());

    displayItemList().beginScope(multicol.displayItemClient());
    drawRect(context, content, foregroundDrawingType, rect3);
    displayItemList().endScope(multicol.displayItemClient());

    // We should repaint everything on invalidation of the scope container.
    EXPECT_TRUE(isDrawing(newPaintListBeforeUpdate()[0]));
    EXPECT_TRUE(isDrawing(newPaintListBeforeUpdate()[1]));
    EXPECT_TRUE(isDrawing(newPaintListBeforeUpdate()[2]));
    EXPECT_TRUE(isDrawing(newPaintListBeforeUpdate()[3]));
    displayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 4,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, displayItemList().displayItems()[1].picture());
    EXPECT_NE(picture2, displayItemList().displayItems()[2].picture());
}

TEST_F(DisplayItemListTest, OptimizeNoopPairs)
{
    TestDisplayItemClient first("first");
    TestDisplayItemClient second("second");
    TestDisplayItemClient third("third");

    GraphicsContext context(&displayItemList());
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipPathRecorder clipRecorder(context, second, Path());
        drawRect(context, second, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    displayItemList().commitNewDisplayItems();
    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 5,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::BeginClipPath),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::EndClipPath),
        TestDisplayItem(third, backgroundDrawingType));

    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, LayoutRect(1, 1, 2, 2));
        // Do not draw anything for second.
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    displayItemList().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));

    displayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, LayoutRect(1, 1, 2, 2));
        {
            ClipPathRecorder clipPathRecorder(context, second, Path());
            // Do not draw anything for second.
        }
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    displayItemList().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(displayItemList().displayItems(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));
}

} // namespace blink
