// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintControllerPaintTest_h
#define PaintControllerPaintTest_h

#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include <gtest/gtest.h>

namespace blink {

class PaintControllerPaintTestBase : public RenderingTest {
public:
    PaintControllerPaintTestBase(bool enableSlimmingPaintV2)
        : m_originalSlimmingPaintOffsetCachingEnabled(RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled())
        , m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        , m_enableSlimmingPaintV2(enableSlimmingPaintV2)
    { }

protected:
    LayoutView& layoutView() { return *document().layoutView(); }
    PaintController& rootPaintController() { return layoutView().layer()->graphicsLayerBacking()->paintController(); }

    void SetUp() override
    {
        RenderingTest::SetUp();
        enableCompositing();
        GraphicsLayer::setDrawDebugRedFillForTesting(false);
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_enableSlimmingPaintV2);
    }
    void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintOffsetCachingEnabled(m_originalSlimmingPaintOffsetCachingEnabled);
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_originalSlimmingPaintV2Enabled);
        GraphicsLayer::setDrawDebugRedFillForTesting(true);
    }

    // Expose some document lifecycle steps for checking new display items before commiting.
    void updateLifecyclePhasesBeforePaint()
    {
        // This doesn't do all steps that FrameView does, but is enough for current tests.
        FrameView* frameView = document().view();
        frameView->updateLifecyclePhasesInternal(FrameView::OnlyUpToCompositingCleanPlusScrolling);
        frameView->invalidateTreeIfNeededRecursive();
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
            document().view()->updatePaintProperties();
    }

    void updateLifecyclePhasesToPaintClean()
    {
        updateLifecyclePhasesBeforePaint();
        document().view()->synchronizedPaint();
    }

    bool paintWithoutCommit(const IntRect* interestRect = nullptr)
    {
        // Only root graphics layer is supported.
        document().view()->lifecycle().advanceTo(DocumentLifecycle::InPaint);
        if (!layoutView().layer()->graphicsLayerBacking()->paintWithoutCommit(interestRect)) {
            document().view()->lifecycle().advanceTo(DocumentLifecycle::PaintClean);
            return false;
        }
        return true;
    }

    void commit()
    {
        // Only root graphics layer is supported.
        rootPaintController().commitNewDisplayItems();
        document().view()->lifecycle().advanceTo(DocumentLifecycle::PaintClean);
    }

    void paint(const IntRect* interestRect = nullptr)
    {
        // Only root graphics layer is supported.
        if (paintWithoutCommit(interestRect))
            commit();
    }

private:
    bool m_originalSlimmingPaintOffsetCachingEnabled;
    bool m_originalSlimmingPaintV2Enabled;
    bool m_enableSlimmingPaintV2;
};

class PaintControllerPaintTest : public PaintControllerPaintTestBase {
public:
    PaintControllerPaintTest() : PaintControllerPaintTestBase(false) { }
};

class PaintControllerPaintTestForSlimmingPaintV2 : public PaintControllerPaintTestBase {
public:
    PaintControllerPaintTestForSlimmingPaintV2() : PaintControllerPaintTestBase(true) { }
};

class PaintControllerPaintTestForSlimmingPaintV1AndV2
    : public PaintControllerPaintTestBase
    , public testing::WithParamInterface<bool> {
public:
    PaintControllerPaintTestForSlimmingPaintV1AndV2() : PaintControllerPaintTestBase(GetParam()) { }
};

class TestDisplayItem final : public DisplayItem {
public:
    TestDisplayItem(const DisplayItemClient& client, Type type) : DisplayItem(client, type, sizeof(*this)) { }

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

// Shorter names for frequently used display item types in tests.
const DisplayItem::Type backgroundType = DisplayItem::BoxDecorationBackground;
const DisplayItem::Type cachedBackgroundType = DisplayItem::drawingTypeToCachedDrawingType(backgroundType);
const DisplayItem::Type foregroundType = DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground);
const DisplayItem::Type cachedForegroundType = DisplayItem::drawingTypeToCachedDrawingType(foregroundType);

} // namespace blink

#endif // PaintControllerPaintTest_h
