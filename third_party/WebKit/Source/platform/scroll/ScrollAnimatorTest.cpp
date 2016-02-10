/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the ScrollAnimator class.

#include "platform/scroll/ScrollAnimator.h"

#include "platform/Logging.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollableArea.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using testing::AtLeast;
using testing::Return;
using testing::_;

static double gMockedTime = 0.0;

static double getMockedTime()
{
    return gMockedTime;
}

namespace {

class MockScrollableArea : public NoBaseWillBeGarbageCollectedFinalized<MockScrollableArea>, public ScrollableArea {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MockScrollableArea);
public:
    static PassOwnPtrWillBeRawPtr<MockScrollableArea> create(bool scrollAnimatorEnabled)
    {
        return adoptPtrWillBeNoop(new MockScrollableArea(scrollAnimatorEnabled));
    }

    MOCK_CONST_METHOD0(isActive, bool());
    MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
    MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
    MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
    MOCK_METHOD2(setScrollOffset, void(const IntPoint&, ScrollType));
    MOCK_METHOD0(scrollControlWasSetNeedsPaintInvalidation, void());
    MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
    MOCK_CONST_METHOD0(minimumScrollPosition, IntPoint());
    MOCK_CONST_METHOD0(maximumScrollPosition, IntPoint());
    MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
    MOCK_CONST_METHOD0(contentsSize, IntSize());
    MOCK_CONST_METHOD0(scrollbarsCanBeActive, bool());
    MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());
    MOCK_METHOD0(registerForAnimation, void());
    MOCK_METHOD0(scheduleAnimation, bool());

    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    IntPoint scrollPosition() const override { return IntPoint(); }
    int visibleHeight() const override { return 768; }
    int visibleWidth() const override { return 1024; }
    bool scrollAnimatorEnabled() const override { return m_scrollAnimatorEnabled; }
    int pageStep(ScrollbarOrientation) const override { return 0; }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

private:
    explicit MockScrollableArea(bool scrollAnimatorEnabled)
        : m_scrollAnimatorEnabled(scrollAnimatorEnabled) { }

    bool m_scrollAnimatorEnabled;
};

} // namespace

static void reset(ScrollAnimator& scrollAnimator)
{
    scrollAnimator.scrollToOffsetWithoutAnimation(FloatPoint());
}

// TODO(skobes): Add unit tests for composited scrolling paths.

TEST(ScrollAnimatorTest, MainThreadStates)
{
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea =
        MockScrollableArea::create(true);
    OwnPtrWillBeRawPtr<ScrollAnimator> scrollAnimator = adoptPtrWillBeNoop(
        new ScrollAnimator(scrollableArea.get(), getMockedTime));

    EXPECT_CALL(*scrollableArea, minimumScrollPosition()).Times(AtLeast(1))
        .WillRepeatedly(Return(IntPoint()));
    EXPECT_CALL(*scrollableArea, maximumScrollPosition()).Times(AtLeast(1))
        .WillRepeatedly(Return(IntPoint(1000, 1000)));
    EXPECT_CALL(*scrollableArea, setScrollOffset(_, _)).Times(2);
    EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(2);
    EXPECT_CALL(*scrollableArea, scheduleAnimation()).Times(AtLeast(1))
        .WillRepeatedly(Return(true));

    // Idle
    EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());
    EXPECT_EQ(scrollAnimator->m_runState,
        ScrollAnimatorCompositorCoordinator::RunState::Idle);

    // WaitingToSendToCompositor
    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByLine, 10, 1);
    EXPECT_EQ(scrollAnimator->m_runState,
        ScrollAnimatorCompositorCoordinator::RunState::WaitingToSendToCompositor);

    // RunningOnMainThread
    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    EXPECT_EQ(scrollAnimator->m_runState,
        ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);
    scrollAnimator->tickAnimation(getMockedTime());
    EXPECT_EQ(scrollAnimator->m_runState,
        ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);

    // PostAnimationCleanup
    scrollAnimator->cancelAnimation();
    EXPECT_EQ(scrollAnimator->m_runState,
        ScrollAnimatorCompositorCoordinator::RunState::PostAnimationCleanup);

    // Idle
    scrollAnimator->updateCompositorAnimations();
    scrollAnimator->tickAnimation(getMockedTime());
    EXPECT_EQ(scrollAnimator->m_runState,
        ScrollAnimatorCompositorCoordinator::RunState::Idle);

    reset(*scrollAnimator);
}

TEST(ScrollAnimatorTest, MainThreadEnabled)
{
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(true);
    OwnPtrWillBeRawPtr<ScrollAnimator> scrollAnimator = adoptPtrWillBeNoop(new ScrollAnimator(scrollableArea.get(), getMockedTime));

    EXPECT_CALL(*scrollableArea, minimumScrollPosition()).Times(AtLeast(1)).WillRepeatedly(Return(IntPoint()));
    EXPECT_CALL(*scrollableArea, maximumScrollPosition()).Times(AtLeast(1)).WillRepeatedly(Return(IntPoint(1000, 1000)));
    EXPECT_CALL(*scrollableArea, setScrollOffset(_, _)).Times(9);
    EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(6);
    EXPECT_CALL(*scrollableArea, scheduleAnimation()).Times(AtLeast(1)).WillRepeatedly(Return(true));

    EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

    ScrollResultOneDimensional result = scrollAnimator->userScroll(HorizontalScrollbar, ScrollByLine, 100, -1);
    EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());
    EXPECT_FALSE(result.didScroll);
    EXPECT_FLOAT_EQ(-1.0f, result.unusedScrollDelta);

    result = scrollAnimator->userScroll(HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
    EXPECT_TRUE(result.didScroll);
    EXPECT_FLOAT_EQ(0.0, result.unusedScrollDelta);

    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    scrollAnimator->tickAnimation(getMockedTime());

    EXPECT_NE(100, scrollAnimator->currentPosition().x());
    EXPECT_NE(0, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByPage, 100, 1);
    EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());

    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    scrollAnimator->tickAnimation(getMockedTime());

    EXPECT_NE(100, scrollAnimator->currentPosition().x());
    EXPECT_NE(0, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByPixel, 4, 25);
    EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());

    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    scrollAnimator->tickAnimation(getMockedTime());

    EXPECT_NE(100, scrollAnimator->currentPosition().x());
    EXPECT_NE(0, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());

    gMockedTime += 1.0;
    scrollAnimator->updateCompositorAnimations();
    scrollAnimator->tickAnimation(getMockedTime());

    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());
    EXPECT_EQ(100, scrollAnimator->currentPosition().x());

    reset(*scrollAnimator);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByPrecisePixel, 4, 25);
    EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

    EXPECT_EQ(100, scrollAnimator->currentPosition().x());
    EXPECT_NE(0, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);
}

// Test that a smooth scroll offset animation is aborted when followed by a
// non-smooth scroll offset animation.
TEST(ScrollAnimatorTest, AnimatedScrollAborted)
{
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea =
        MockScrollableArea::create(true);
    OwnPtrWillBeRawPtr<ScrollAnimator> scrollAnimator = adoptPtrWillBeNoop(
        new ScrollAnimator(scrollableArea.get(), getMockedTime));

    EXPECT_CALL(*scrollableArea, minimumScrollPosition()).Times(AtLeast(1))
        .WillRepeatedly(Return(IntPoint()));
    EXPECT_CALL(*scrollableArea, maximumScrollPosition()).Times(AtLeast(1))
        .WillRepeatedly(Return(IntPoint(1000, 1000)));
    EXPECT_CALL(*scrollableArea, setScrollOffset(_, _)).Times(3);
    EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(2);
    EXPECT_CALL(*scrollableArea, scheduleAnimation()).Times(AtLeast(1))
        .WillRepeatedly(Return(true));

    EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

    // Smooth scroll.
    ScrollResultOneDimensional result = scrollAnimator->userScroll(
        HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
    EXPECT_TRUE(result.didScroll);
    EXPECT_FLOAT_EQ(0.0, result.unusedScrollDelta);
    EXPECT_TRUE(scrollAnimator->hasRunningAnimation());

    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    scrollAnimator->tickAnimation(getMockedTime());

    EXPECT_NE(100, scrollAnimator->currentPosition().x());
    EXPECT_NE(0, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());

    float x = scrollAnimator->currentPosition().x();

    // Instant scroll.
    result = scrollAnimator->userScroll(
        HorizontalScrollbar, ScrollByPrecisePixel, 100, 1);
    EXPECT_TRUE(result.didScroll);
    gMockedTime += 0.05;
    scrollAnimator->updateCompositorAnimations();
    EXPECT_FALSE(scrollAnimator->hasRunningAnimation());
    EXPECT_EQ(x + 100, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());

    reset(*scrollAnimator);
}

TEST(ScrollAnimatorTest, Disabled)
{
    OwnPtrWillBeRawPtr<MockScrollableArea> scrollableArea = MockScrollableArea::create(false);
    OwnPtrWillBeRawPtr<ScrollAnimator> scrollAnimator = adoptPtrWillBeNoop(new ScrollAnimator(scrollableArea.get(), getMockedTime));

    EXPECT_CALL(*scrollableArea, minimumScrollPosition()).Times(AtLeast(1)).WillRepeatedly(Return(IntPoint()));
    EXPECT_CALL(*scrollableArea, maximumScrollPosition()).Times(AtLeast(1)).WillRepeatedly(Return(IntPoint(1000, 1000)));
    EXPECT_CALL(*scrollableArea, setScrollOffset(_, _)).Times(8);
    EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(0);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_EQ(100, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByPage, 100, 1);
    EXPECT_EQ(100, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByDocument, 100, 1);
    EXPECT_EQ(100, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);

    scrollAnimator->userScroll(HorizontalScrollbar, ScrollByPixel, 100, 1);
    EXPECT_EQ(100, scrollAnimator->currentPosition().x());
    EXPECT_EQ(0, scrollAnimator->currentPosition().y());
    reset(*scrollAnimator);
}

} // namespace blink
