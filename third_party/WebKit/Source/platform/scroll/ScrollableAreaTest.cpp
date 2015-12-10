// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scroll/ScrollableArea.h"

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
    FakePlatform() { }
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
        Platform::initialize(&m_fakePlatform);
    }

    void TearDown() override
    {
        Platform::initialize(m_oldPlatform);
    }

private:
    FakePlatform m_fakePlatform;
    Platform* m_oldPlatform; // Not owned.
};

TEST_F(ScrollableAreaTest, ScrollAnimatorCurrentPositionShouldBeSync)
{
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    scrollableArea->setScrollPosition(IntPoint(0, 10000), CompositorScroll);
    EXPECT_EQ(100.0, scrollableArea->scrollAnimator().currentPosition().y());
}

TEST_F(ScrollableAreaTest, ScrollbarTrackAndThumbRepaint)
{
    ScrollbarTheme::setMockScrollbarsEnabled(true);
    ScrollbarThemeMock::setShouldRepaintAllPartsOnInvalidation(true);

    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    RefPtrWillBeRawPtr<Scrollbar> scrollbar = Scrollbar::create(scrollableArea.get(), HorizontalScrollbar, RegularScrollbar);

    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation();
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    scrollbar->setTrackNeedsRepaint(false);
    scrollbar->setThumbNeedsRepaint(false);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation();
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    ScrollbarThemeMock::setShouldRepaintAllPartsOnInvalidation(false);
    scrollbar->setTrackNeedsRepaint(false);
    scrollbar->setThumbNeedsRepaint(false);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation();
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
}

} // namespace blink
