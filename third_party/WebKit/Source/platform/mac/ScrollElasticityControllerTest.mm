// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <gtest/gtest.h>

#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntSize.h"
#include "platform/mac/ScrollElasticityController.h"
#include "platform/PlatformWheelEvent.h"

namespace WebCore {

class MockScrollElasticityControllerClient : public ScrollElasticityControllerClient {
public:
    MockScrollElasticityControllerClient() : m_pinned(true) {}

    bool allowsHorizontalStretching() { return true; }
    bool allowsVerticalStretching() { return true; }
    // The amount that the view is stretched past the normal allowable bounds.
    // The "overhang" amount.
    IntSize stretchAmount() { return IntSize(m_stretchX,0); }
    bool pinnedInDirection(const FloatSize&) { return m_pinned; }
    bool canScrollHorizontally() { return true; }
    bool canScrollVertically() { return true; }

    // Return the absolute scroll position, not relative to the scroll origin.
    WebCore::IntPoint absoluteScrollPosition() { return IntPoint(m_stretchX,0); }

    void immediateScrollBy(const FloatSize& size) { m_stretchX += size.width(); }
    void immediateScrollByWithoutContentEdgeConstraints(const FloatSize& size) { m_stretchX += size.width(); }
    void startSnapRubberbandTimer() {}
    void stopSnapRubberbandTimer() {}

    void reset() { m_stretchX = 0; }

    bool m_pinned;

private:
    float m_stretchX;
};

class MockPlatformWheelEvent : public PlatformWheelEvent {
public:
    MockPlatformWheelEvent(PlatformWheelEventPhase phase, float deltaX, bool canRubberbandLeft)
    {
        m_phase = phase;
        m_deltaX = deltaX;
        m_canRubberbandLeft = canRubberbandLeft;
    }
};

enum RubberbandPermission {
    DisallowRubberband,
    AllowRubberband
};

enum ScrollPermission {
    DisallowScroll,
    AllowScroll
};

const float deltaLeft = 1;
const float deltaNone = 0;

class ScrollElasticityControllerTest : public testing::Test {
public:
    ScrollElasticityControllerTest() : m_client(), m_controller(&m_client) {}

    // Makes a wheel event with the given parameters, and forwards the event to the
    // controller.
    bool handleWheelEvent(RubberbandPermission canRubberband, ScrollPermission canScroll, PlatformWheelEventPhase phase, float delta)
    {
        m_client.m_pinned = !canScroll;
        MockPlatformWheelEvent event(phase, delta, canRubberband);
        return m_controller.handleWheelEvent(event);
    }

    void SetUp()
    {
        m_client.reset();
    }

private:
    MockScrollElasticityControllerClient m_client;
    ScrollElasticityController m_controller;
};

// The client cannot scroll, but the event allows rubber banding.
TEST_F(ScrollElasticityControllerTest, canRubberband)
{
    // The PlatformWheelEventPhaseMayBegin event should never be handled.
    EXPECT_FALSE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseMayBegin, deltaNone));

    // All other events should be handled.
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseBegan, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseEnded, deltaNone));
}

// The client cannot scroll, and the event disallows rubber banding.
TEST_F(ScrollElasticityControllerTest, cannotRubberband)
{
    // The PlatformWheelEventPhaseMayBegin event should never be handled.
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseMayBegin, deltaNone));

    // Rubber-banding is disabled, and the client is pinned.
    // Ignore all events.
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseBegan, deltaLeft));
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseEnded, deltaNone));
}

// The client can scroll, and the event disallows rubber banding.
TEST_F(ScrollElasticityControllerTest, cannotRubberbandCanScroll)
{
    // The PlatformWheelEventPhaseMayBegin event should never be handled.
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, AllowScroll, PlatformWheelEventPhaseMayBegin, deltaNone));

    // Rubber-banding is disabled, but the client is not pinned.
    // Handle all events.
    EXPECT_TRUE(handleWheelEvent(DisallowRubberband, AllowScroll, PlatformWheelEventPhaseBegan, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(DisallowRubberband, AllowScroll, PlatformWheelEventPhaseChanged, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(DisallowRubberband, AllowScroll, PlatformWheelEventPhaseEnded, deltaNone));
}

// The client cannot scroll. The initial events allow rubber banding, but the
// later events disallow it.
TEST_F(ScrollElasticityControllerTest, canRubberbandBecomesFalse)
{
    // The PlatformWheelEventPhaseMayBegin event should never be handled.
    EXPECT_FALSE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseMayBegin, deltaNone));

    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseBegan, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));

    // Rubber-banding is no longer allowed, but its already started.
    // Events should still be handled.
    EXPECT_TRUE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseEnded, deltaNone));
}

// The client cannot scroll. The initial events disallow rubber banding, but the
// later events allow it.
TEST_F(ScrollElasticityControllerTest, canRubberbandBecomesTrue)
{
    // The PlatformWheelEventPhaseMayBegin event should never be handled.
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseMayBegin, deltaNone));

    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseBegan, deltaLeft));
    EXPECT_FALSE(handleWheelEvent(DisallowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));

    // Rubber-banding is now allowed
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseEnded, deltaNone));
}

// Events with no delta should not cause scrolling.
TEST_F(ScrollElasticityControllerTest, zeroDeltaEventsIgnored)
{
    // The PlatformWheelEventPhaseMayBegin event should never be handled.
    EXPECT_FALSE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseMayBegin, deltaNone));

    EXPECT_FALSE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseBegan, deltaNone));
    EXPECT_FALSE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaNone));
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseChanged, deltaLeft));
    EXPECT_TRUE(handleWheelEvent(AllowRubberband, DisallowScroll, PlatformWheelEventPhaseEnded, deltaNone));
}

} // namespace WebCore
