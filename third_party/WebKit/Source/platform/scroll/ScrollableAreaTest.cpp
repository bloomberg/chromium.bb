// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollableArea.h"

#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using testing::Return;

class MockScrollableArea : public NoBaseWillBeGarbageCollectedFinalized<MockScrollableArea>, public ScrollableArea {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MockScrollableArea);
public:
    static PassOwnPtrWillBeRawPtr<MockScrollableArea> create(const IntPoint& maximumScrollPosition)
    {
        return adoptPtrWillBeNoop(new MockScrollableArea(maximumScrollPosition));
    }

    MOCK_CONST_METHOD0(isActive, bool());
    MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
    MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
    MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
    MOCK_METHOD0(scrollControlWasSetNeedsPaintInvalidation, void());
    MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
    MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
    MOCK_CONST_METHOD0(contentsSize, IntSize());
    MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());
    MOCK_CONST_METHOD0(layerForHorizontalScrollbar, GraphicsLayer*());

    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool scrollbarsCanBeActive () const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    void setScrollOffset(const IntPoint& offset, ScrollType) override { m_scrollPosition = offset.shrunkTo(m_maximumScrollPosition); }
    IntPoint scrollPosition() const override { return m_scrollPosition; }
    IntPoint minimumScrollPosition() const override { return IntPoint(); }
    IntPoint maximumScrollPosition() const override { return m_maximumScrollPosition; }
    int visibleHeight() const override { return 768; }
    int visibleWidth() const override { return 1024; }
    bool scrollAnimatorEnabled() const override { return false; }
    int pageStep(ScrollbarOrientation) const override { return 0; }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

private:
    explicit MockScrollableArea(const IntPoint& maximumScrollPosition)
        : m_maximumScrollPosition(maximumScrollPosition) { }

    IntPoint m_scrollPosition;
    IntPoint m_maximumScrollPosition;
};

} // namespace

class ScrollableAreaTest : public testing::Test {
public:
    ScrollableAreaTest() : m_oldPlatform(nullptr) { }

    void SetUp() override
    {
        m_oldPlatform = Platform::current();
        TestingPlatformSupport::Config config;
        config.compositorSupport = m_oldPlatform->compositorSupport();
        m_fakePlatform = adoptPtr(new TestingPlatformSupportWithMockScheduler(config));
        Platform::initialize(m_fakePlatform.get());
    }

    void TearDown() override
    {
        Platform::initialize(m_oldPlatform);
        m_fakePlatform = nullptr;
    }

private:
    OwnPtr<TestingPlatformSupportWithMockScheduler> m_fakePlatform;
    Platform* m_oldPlatform; // Not owned.
};

TEST_F(ScrollableAreaTest, ScrollAnimatorCurrentPositionShouldBeSync)
{
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    scrollableArea->setScrollPosition(IntPoint(0, 10000), CompositorScroll);
    EXPECT_EQ(100.0, scrollableArea->scrollAnimator().currentPosition().y());
}

namespace {

class ScrollbarThemeWithMockInvalidation : public ScrollbarThemeMock {
public:
    MOCK_CONST_METHOD0(shouldRepaintAllPartsOnInvalidation, bool());
};

} // namespace

TEST_F(ScrollableAreaTest, ScrollbarTrackAndThumbRepaint)
{
    ScrollbarThemeWithMockInvalidation theme;
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    RefPtrWillBeRawPtr<Scrollbar> scrollbar = Scrollbar::createForTesting(scrollableArea.get(), HorizontalScrollbar, RegularScrollbar, &theme);

    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(true));
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(NoPart);
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    scrollbar->clearTrackNeedsRepaint();
    scrollbar->clearThumbNeedsRepaint();
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(NoPart);
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));
    scrollbar->clearTrackNeedsRepaint();
    scrollbar->clearThumbNeedsRepaint();
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(NoPart);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());

    // Forced GC in order to finalize objects depending on the mock object.
    Heap::collectAllGarbage();
}

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    IntRect computeInterestRect(const GraphicsLayer*, const IntRect&) const { return IntRect(); }
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) const override { }
    String debugName(const GraphicsLayer*) const override { return String(); }
    bool isTrackingPaintInvalidations() const override { return true; }
};

class MockGraphicsLayer : public GraphicsLayer {
public:
    explicit MockGraphicsLayer(GraphicsLayerClient* client) : GraphicsLayer(client) { }
};

TEST_F(ScrollableAreaTest, ScrollbarGraphicsLayerInvalidation)
{
    ScrollbarTheme::setMockScrollbarsEnabled(true);
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    MockGraphicsLayerClient graphicsLayerClient;
    MockGraphicsLayer graphicsLayer(&graphicsLayerClient);
    graphicsLayer.setDrawsContent(true);
    graphicsLayer.setSize(FloatSize(111, 222));

    EXPECT_CALL(*scrollableArea, layerForHorizontalScrollbar()).WillRepeatedly(Return(&graphicsLayer));

    RefPtrWillBeRawPtr<Scrollbar> scrollbar = Scrollbar::create(scrollableArea.get(), HorizontalScrollbar, RegularScrollbar, nullptr);
    graphicsLayer.resetTrackedPaintInvalidations();
    scrollbar->setNeedsPaintInvalidation(NoPart);
    EXPECT_TRUE(graphicsLayer.hasTrackedPaintInvalidations());
}

} // namespace blink
