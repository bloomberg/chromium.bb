// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/scroll/ScrollableArea.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace blink;

namespace {

class MockScrollableArea : public ScrollableArea {
public:
    MockScrollableArea(const IntPoint& maximumScrollPosition)
        : m_maximumScrollPosition(maximumScrollPosition) { }

    MOCK_CONST_METHOD0(isActive, bool());
    MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
    MOCK_METHOD2(invalidateScrollbar, void(Scrollbar*, const IntRect&));
    MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
    MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
    MOCK_METHOD2(invalidateScrollbarRect, void(Scrollbar*, const IntRect&));
    MOCK_METHOD1(invalidateScrollCornerRect, void(const IntRect&));
    MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
    MOCK_CONST_METHOD0(minimumScrollPosition, IntPoint());
    MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
    MOCK_CONST_METHOD0(contentsSize, IntSize());
    MOCK_CONST_METHOD0(overhangAmount, IntSize());
    MOCK_CONST_METHOD0(scrollbarsCanBeActive, bool());
    MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());

    virtual bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    virtual void setScrollOffset(const IntPoint& offset) override { m_scrollPosition = offset.shrunkTo(m_maximumScrollPosition); }
    virtual IntPoint scrollPosition() const override { return m_scrollPosition; }
    virtual IntPoint maximumScrollPosition() const override { return m_maximumScrollPosition; }
    virtual int visibleHeight() const override { return 768; }
    virtual int visibleWidth() const override { return 1024; }
    virtual bool scrollAnimatorEnabled() const override { return false; }
    virtual int pageStep(ScrollbarOrientation) const override { return 0; }

private:
    IntPoint m_scrollPosition;
    IntPoint m_maximumScrollPosition;
};

TEST(ScrollableAreaTest, ScrollAnimatorCurrentPositionShouldBeSync)
{
    MockScrollableArea scrollableArea(IntPoint(0, 100));
    scrollableArea.notifyScrollPositionChanged(IntPoint(0, 10000));
    EXPECT_EQ(100.0, scrollableArea.scrollAnimator()->currentPosition().y());
}

} // unnamed namespace


