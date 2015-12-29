// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollableArea.h"

#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using testing::_;
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
    MOCK_CONST_METHOD0(horizontalScrollbar, Scrollbar*());
    MOCK_CONST_METHOD0(verticalScrollbar, Scrollbar*());
    MOCK_METHOD0(scrollControlWasSetNeedsPaintInvalidation, void());
    MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
    MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
    MOCK_CONST_METHOD0(contentsSize, IntSize());
    MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());
    MOCK_CONST_METHOD0(layerForHorizontalScrollbar, GraphicsLayer*());
    MOCK_CONST_METHOD0(layerForVerticalScrollbar, GraphicsLayer*());

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

    using ScrollableArea::horizontalScrollbarNeedsPaintInvalidation;
    using ScrollableArea::verticalScrollbarNeedsPaintInvalidation;
    using ScrollableArea::clearNeedsPaintInvalidationForScrollControls;

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

class FakeWebThread : public WebThread {
public:
    FakeWebThread() { }
    ~FakeWebThread() override { }

    WebTaskRunner* taskRunner() override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    bool isCurrentThread() const override
    {
        ASSERT_NOT_REACHED();
        return true;
    }

    WebScheduler* scheduler() const override
    {
        return nullptr;
    }
};

// The FakePlatform is needed because ScrollAnimatorMac's constructor creates several timers.
// We need just enough scaffolding for the Timer constructor to not segfault.
class FakePlatform : public TestingPlatformSupport {
public:
    explicit FakePlatform(Config& config) : TestingPlatformSupport(config) { }

    ~FakePlatform() override { }

    WebThread* currentThread() override
    {
        return &m_webThread;
    }

private:
    FakeWebThread m_webThread;
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
        m_fakePlatform = adoptPtr(new FakePlatform(config));
        Platform::initialize(m_fakePlatform.get());
    }

    void TearDown() override
    {
        Platform::initialize(m_oldPlatform);
        m_fakePlatform = nullptr;
    }

private:
    OwnPtr<FakePlatform> m_fakePlatform;
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
    MOCK_CONST_METHOD3(invalidateOnThumbPositionChange, ScrollbarPart(const ScrollbarThemeClient&, float, float));
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
    scrollbar->setNeedsPaintInvalidation();
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    scrollbar->setTrackNeedsRepaint(false);
    scrollbar->setThumbNeedsRepaint(false);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(ThumbPart);
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    // When not all parts are repainted on invalidation,
    // setNeedsPaintInvalidation sets repaint bits only on the requested parts.
    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));
    scrollbar->setTrackNeedsRepaint(false);
    scrollbar->setThumbNeedsRepaint(false);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(ThumbPart);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

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

    RefPtrWillBeRawPtr<Scrollbar> scrollbar = Scrollbar::create(scrollableArea.get(), HorizontalScrollbar, RegularScrollbar);
    graphicsLayer.resetTrackedPaintInvalidations();
    scrollbar->setNeedsPaintInvalidation();
    EXPECT_TRUE(graphicsLayer.hasTrackedPaintInvalidations());
}

TEST_F(ScrollableAreaTest, InvalidatesNonCompositedScrollbarsWhenThumbMoves)
{
    ScrollbarThemeWithMockInvalidation theme;
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(100, 100));
    RefPtrWillBeRawPtr<Scrollbar> horizontalScrollbar = Scrollbar::createForTesting(scrollableArea.get(), HorizontalScrollbar, RegularScrollbar, &theme);
    RefPtrWillBeRawPtr<Scrollbar> verticalScrollbar = Scrollbar::createForTesting(scrollableArea.get(), VerticalScrollbar, RegularScrollbar, &theme);
    EXPECT_CALL(*scrollableArea, horizontalScrollbar()).WillRepeatedly(Return(horizontalScrollbar.get()));
    EXPECT_CALL(*scrollableArea, verticalScrollbar()).WillRepeatedly(Return(verticalScrollbar.get()));

    // Regardless of whether the theme invalidates any parts, non-composited
    // scrollbars have to be repainted if the thumb moves.
    EXPECT_CALL(*scrollableArea, layerForHorizontalScrollbar()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*scrollableArea, layerForVerticalScrollbar()).WillRepeatedly(Return(nullptr));
    ASSERT_FALSE(scrollableArea->hasLayerForVerticalScrollbar());
    ASSERT_FALSE(scrollableArea->hasLayerForHorizontalScrollbar());
    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).WillRepeatedly(Return(NoPart));

    // A scroll in each direction should only invalidate one scrollbar.
    scrollableArea->setScrollPosition(DoublePoint(0, 50), ProgrammaticScroll);
    EXPECT_FALSE(scrollableArea->horizontalScrollbarNeedsPaintInvalidation());
    EXPECT_TRUE(scrollableArea->verticalScrollbarNeedsPaintInvalidation());
    scrollableArea->clearNeedsPaintInvalidationForScrollControls();
    scrollableArea->setScrollPosition(DoublePoint(50, 50), ProgrammaticScroll);
    EXPECT_TRUE(scrollableArea->horizontalScrollbarNeedsPaintInvalidation());
    EXPECT_FALSE(scrollableArea->verticalScrollbarNeedsPaintInvalidation());
    scrollableArea->clearNeedsPaintInvalidationForScrollControls();

    // Forced GC in order to finalize objects depending on the mock object.
    Heap::collectAllGarbage();
}


TEST_F(ScrollableAreaTest, InvalidatesCompositedScrollbarsIfPartsNeedRepaint)
{
    ScrollbarThemeWithMockInvalidation theme;
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(100, 100));
    RefPtrWillBeRawPtr<Scrollbar> horizontalScrollbar = Scrollbar::createForTesting(scrollableArea.get(), HorizontalScrollbar, RegularScrollbar, &theme);
    horizontalScrollbar->setTrackNeedsRepaint(false);
    horizontalScrollbar->setThumbNeedsRepaint(false);
    RefPtrWillBeRawPtr<Scrollbar> verticalScrollbar = Scrollbar::createForTesting(scrollableArea.get(), VerticalScrollbar, RegularScrollbar, &theme);
    verticalScrollbar->setTrackNeedsRepaint(false);
    verticalScrollbar->setThumbNeedsRepaint(false);
    EXPECT_CALL(*scrollableArea, horizontalScrollbar()).WillRepeatedly(Return(horizontalScrollbar.get()));
    EXPECT_CALL(*scrollableArea, verticalScrollbar()).WillRepeatedly(Return(verticalScrollbar.get()));

    // Composited scrollbars only need repainting when parts become invalid
    // (e.g. if the track changes appearance when the thumb reaches the end).
    MockGraphicsLayerClient graphicsLayerClient;
    MockGraphicsLayer layerForHorizontalScrollbar(&graphicsLayerClient);
    layerForHorizontalScrollbar.setDrawsContent(true);
    layerForHorizontalScrollbar.setSize(FloatSize(10, 10));
    MockGraphicsLayer layerForVerticalScrollbar(&graphicsLayerClient);
    layerForVerticalScrollbar.setDrawsContent(true);
    layerForVerticalScrollbar.setSize(FloatSize(10, 10));
    EXPECT_CALL(*scrollableArea, layerForHorizontalScrollbar()).WillRepeatedly(Return(&layerForHorizontalScrollbar));
    EXPECT_CALL(*scrollableArea, layerForVerticalScrollbar()).WillRepeatedly(Return(&layerForVerticalScrollbar));
    ASSERT_TRUE(scrollableArea->hasLayerForHorizontalScrollbar());
    ASSERT_TRUE(scrollableArea->hasLayerForVerticalScrollbar());
    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));

    // First, we'll scroll horizontally, and the theme will require repainting
    // the back button (i.e. the track).
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).WillOnce(Return(BackButtonStartPart));
    scrollableArea->setScrollPosition(DoublePoint(50, 0), ProgrammaticScroll);
    EXPECT_TRUE(layerForHorizontalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(layerForVerticalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_TRUE(horizontalScrollbar->trackNeedsRepaint());
    EXPECT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
    layerForHorizontalScrollbar.resetTrackedPaintInvalidations();
    horizontalScrollbar->setTrackNeedsRepaint(false);

    // Next, we'll scroll vertically, but invalidate the thumb.
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).WillOnce(Return(ThumbPart));
    scrollableArea->setScrollPosition(DoublePoint(50, 50), ProgrammaticScroll);
    EXPECT_FALSE(layerForHorizontalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_TRUE(layerForVerticalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(verticalScrollbar->trackNeedsRepaint());
    EXPECT_TRUE(verticalScrollbar->thumbNeedsRepaint());
    layerForVerticalScrollbar.resetTrackedPaintInvalidations();
    verticalScrollbar->setThumbNeedsRepaint(false);

    // Next we'll scroll in both, but the thumb position moving requires no
    // invalidations.
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).Times(2).WillRepeatedly(Return(NoPart));
    scrollableArea->setScrollPosition(DoublePoint(70, 70), ProgrammaticScroll);
    EXPECT_FALSE(layerForHorizontalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(layerForVerticalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(horizontalScrollbar->trackNeedsRepaint());
    EXPECT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
    EXPECT_FALSE(verticalScrollbar->trackNeedsRepaint());
    EXPECT_FALSE(verticalScrollbar->thumbNeedsRepaint());

    // Forced GC in order to finalize objects depending on the mock object.
    Heap::collectAllGarbage();
}

} // namespace blink
